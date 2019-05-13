// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/promise/abstract_promise.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace base {
namespace internal {

AbstractPromise::~AbstractPromise() {
#if DCHECK_IS_ON()
  CheckedAutoLock lock(GetCheckedLock());

  DCHECK(!must_catch_ancestor_that_could_reject_ ||
         passed_catch_responsibility_)
      << "Promise chain ending at " << from_here_.ToString()
      << " didn't have a catch for potentially rejecting promise here "
      << must_catch_ancestor_that_could_reject_->from_here().ToString();

  DCHECK(!this_must_catch_ || passed_catch_responsibility_)
      << "Potentially rejecting promise at " << from_here_.ToString()
      << " doesn't have a catch .";
#endif
}

bool AbstractPromise::IsCanceled() const {
  if (dependents_.IsCanceled())
    return true;

  const Executor* executor = GetExecutor();
  return executor && executor->IsCancelled();
}

const AbstractPromise* AbstractPromise::FindNonCurriedAncestor() const {
  const AbstractPromise* promise = this;
  while (promise->IsResolvedWithPromise()) {
    promise =
        unique_any_cast<scoped_refptr<AbstractPromise>>(promise->value_).get();
  }
  return promise;
}

void AbstractPromise::AddAsDependentForAllPrerequisites() {
  DCHECK(prerequisites_);

  for (AdjacencyListNode& node : prerequisites_->prerequisite_list) {
    node.dependent_node.dependent = this;

    // If |node.prerequisite| was canceled then early out because
    // |prerequisites_->prerequisite_list| will have been cleared.
    if (!node.prerequisite->InsertDependentOnAnyThread(&node.dependent_node))
      break;
  }
}

bool AbstractPromise::InsertDependentOnAnyThread(DependentList::Node* node) {
  scoped_refptr<AbstractPromise>& dependent = node->dependent;

#if DCHECK_IS_ON()
  {
    CheckedAutoLock lock(GetCheckedLock());
    node->dependent->MaybeInheritChecks(this);
  }
#endif

  // If |dependents_| has been consumed (i.e. this promise has been resolved
  // or rejected) then |node| may be ready to run now.
  switch (dependents_.Insert(node)) {
    case DependentList::InsertResult::SUCCESS:
      break;

    case DependentList::InsertResult::FAIL_PROMISE_RESOLVED:
      dependent->OnPrerequisiteResolved();
      break;

    case DependentList::InsertResult::FAIL_PROMISE_REJECTED:
      dependent->OnPrerequisiteRejected();
      break;

    case DependentList::InsertResult::FAIL_PROMISE_CANCELED:
      return dependent->OnPrerequisiteCancelled();
  }

  return true;
}

#if DCHECK_IS_ON()

// static
CheckedLock& AbstractPromise::GetCheckedLock() {
  static base::NoDestructor<CheckedLock> instance;
  return *instance;
}

void AbstractPromise::DoubleMoveDetector::CheckForDoubleMoveErrors(
    const base::Location& new_dependent_location,
    Executor::ArgumentPassingType new_dependent_executor_type) {
  switch (new_dependent_executor_type) {
    case Executor::ArgumentPassingType::kNoCallback:
      return;

    case Executor::ArgumentPassingType::kNormal:
      DCHECK(!dependent_move_only_promise_)
          << "Can't mix move only and non-move only " << callback_type_
          << "callback arguments for the same " << callback_type_
          << " prerequisite. See " << new_dependent_location.ToString()
          << " and " << dependent_move_only_promise_->ToString()
          << " with common ancestor " << from_here_.ToString();
      dependent_normal_promise_ =
          std::make_unique<Location>(new_dependent_location);
      return;

    case Executor::ArgumentPassingType::kMove:
      DCHECK(!dependent_move_only_promise_)
          << "Can't have multiple move only " << callback_type_
          << " callbacks for same " << callback_type_ << " prerequisite. See "
          << new_dependent_location.ToString() << " and "
          << dependent_move_only_promise_->ToString() << " with common "
          << callback_type_ << " prerequisite " << from_here_.ToString();
      DCHECK(!dependent_normal_promise_)
          << "Can't mix move only and non-move only " << callback_type_
          << " callback arguments for the same " << callback_type_
          << " prerequisite. See " << new_dependent_location.ToString()
          << " and " << dependent_normal_promise_->ToString() << " with common "
          << callback_type_ << " prerequisite " << from_here_.ToString();
      dependent_move_only_promise_ =
          std::make_unique<Location>(new_dependent_location);
      return;
  }
}

void AbstractPromise::MaybeInheritChecks(AbstractPromise* prerequisite) {
  if (!ancestor_that_could_resolve_) {
    // Inherit |prerequisite|'s resolve ancestor if it doesn't have a resolve
    // callback.
    if (prerequisite->resolve_argument_passing_type_ ==
        Executor::ArgumentPassingType::kNoCallback) {
      ancestor_that_could_resolve_ = prerequisite->ancestor_that_could_resolve_;
    }

    // If |prerequisite| didn't have a resolve callback (but it's reject
    // callback could resolve) or if
    // |prerequisite->ancestor_that_could_resolve_| is null then assign
    // |prerequisite->this_resolve_|.
    if (!ancestor_that_could_resolve_ && prerequisite->executor_can_resolve_)
      ancestor_that_could_resolve_ = prerequisite->this_resolve_;
  }

  if (!ancestor_that_could_reject_) {
    // Inherit |prerequisite|'s reject ancestor if it doesn't have a Catch.
    if (prerequisite->reject_argument_passing_type_ ==
        Executor::ArgumentPassingType::kNoCallback) {
      ancestor_that_could_reject_ = prerequisite->ancestor_that_could_reject_;
    }

    // If |prerequisite| didn't have a reject callback (but it's resolve
    // callback could reject) or if
    // |prerequisite->ancestor_that_could_resolve_| is null then assign
    // |prerequisite->this_reject_|.
    if (!ancestor_that_could_reject_ && prerequisite->executor_can_reject_)
      ancestor_that_could_reject_ = prerequisite->this_reject_;
  }

  if (!must_catch_ancestor_that_could_reject_) {
    // Inherit |prerequisite|'s must catch ancestor if it doesn't have a Catch.
    if (prerequisite->reject_argument_passing_type_ ==
        Executor::ArgumentPassingType::kNoCallback) {
      must_catch_ancestor_that_could_reject_ =
          prerequisite->must_catch_ancestor_that_could_reject_;
    }

    // If |prerequisite| didn't have a reject callback (but it's resolve
    // callback could reject) or if
    // |prerequisite->must_catch_ancestor_that_could_reject_| is null then
    // assign |prerequisite->this_must_catch_|.
    if (!must_catch_ancestor_that_could_reject_ &&
        prerequisite->executor_can_reject_) {
      must_catch_ancestor_that_could_reject_ = prerequisite->this_must_catch_;
    }
  }

  if (ancestor_that_could_resolve_) {
    ancestor_that_could_resolve_->CheckForDoubleMoveErrors(
        from_here_, resolve_argument_passing_type_);
  }

  if (ancestor_that_could_reject_) {
    ancestor_that_could_reject_->CheckForDoubleMoveErrors(
        from_here_, reject_argument_passing_type_);
  }

  prerequisite->passed_catch_responsibility_ = true;
}

void AbstractPromise::PassCatchResponsibilityOntoDependentsForCurriedPromise(
    DependentList::Node* dependent_list) {
  CheckedAutoLock lock(GetCheckedLock());
  if (!dependent_list)
    return;

  if (IsResolvedWithPromise()) {
    for (DependentList::Node* node = dependent_list; node;
         node = node->next.load(std::memory_order_relaxed)) {
      node->dependent->MaybeInheritChecks(this);
    }
  }
}

AbstractPromise::LocationRef::LocationRef(const Location& from_here)
    : from_here_(from_here) {}

AbstractPromise::LocationRef::~LocationRef() = default;

AbstractPromise::DoubleMoveDetector::DoubleMoveDetector(
    const Location& from_here,
    const char* callback_type)
    : from_here_(from_here), callback_type_(callback_type) {}

AbstractPromise::DoubleMoveDetector::~DoubleMoveDetector() = default;

#endif

const AbstractPromise::Executor* AbstractPromise::GetExecutor() const {
  const SmallUniqueObject<Executor>* executor =
      base::unique_any_cast<SmallUniqueObject<Executor>>(&value_);
  if (!executor)
    return nullptr;
  return executor->get();
}

AbstractPromise::Executor::PrerequisitePolicy
AbstractPromise::GetPrerequisitePolicy() {
  Executor* executor = GetExecutor();
  if (!executor) {
    // If there's no executor it's because the promise has already run. We
    // can't run again however. The only circumstance in which we expect
    // GetPrerequisitePolicy() to be called after execution is when it was
    // resolved with a promise.
    DCHECK(IsResolvedWithPromise());
    return Executor::PrerequisitePolicy::kNever;
  }
  return executor->GetPrerequisitePolicy();
}

void AbstractPromise::Execute() {
  if (IsCanceled())
    return;

#if DCHECK_IS_ON()
  // Clear |must_catch_ancestor_that_could_reject_| if we can catch it.
  if (reject_argument_passing_type_ !=
      Executor::ArgumentPassingType::kNoCallback) {
    CheckedAutoLock lock(GetCheckedLock());
    must_catch_ancestor_that_could_reject_ = nullptr;
  }
#endif

  if (IsResolvedWithPromise()) {
    bool settled = DispatchIfNonCurriedRootSettled();
    DCHECK(settled);

    prerequisites_->prerequisite_list.clear();
    return;
  }

  DCHECK(GetExecutor()) << from_here_.ToString() << " value_ contains "
                        << value_.type();

  // This is likely to delete the executor.
  GetExecutor()->Execute(this);

  // We need to release any AdjacencyListNodes we own to prevent memory leaks
  // due to refcount cycles.
  if (prerequisites_ && !IsResolvedWithPromise())
    prerequisites_->prerequisite_list.clear();
}

bool AbstractPromise::DispatchIfNonCurriedRootSettled() {
  const AbstractPromise* curried_root = FindNonCurriedAncestor();
  if (!curried_root->IsSettled())
    return false;

  if (curried_root->IsResolved()) {
    OnResolvePostReadyDependents();
  } else if (curried_root->IsRejected()) {
    OnRejectPostReadyDependents();
  } else {
    DCHECK(curried_root->IsCanceled());
    OnPrerequisiteCancelled();
  }
  return true;
}

void AbstractPromise::OnPrerequisiteResolved() {
  if (IsResolvedWithPromise()) {
    bool settled = DispatchIfNonCurriedRootSettled();
    DCHECK(settled);
    return;
  }

  switch (GetPrerequisitePolicy()) {
    case Executor::PrerequisitePolicy::kAll:
      if (prerequisites_->DecrementPrerequisiteCountAndCheckIfZero())
        task_runner_->PostPromiseInternal(this, TimeDelta());
      break;

    case Executor::PrerequisitePolicy::kAny:
      // PrerequisitePolicy::kAny should resolve immediately.
      task_runner_->PostPromiseInternal(this, TimeDelta());
      break;

    case Executor::PrerequisitePolicy::kNever:
      break;
  }
}

void AbstractPromise::OnPrerequisiteRejected() {
  task_runner_->PostPromiseInternal(this, TimeDelta());
}

bool AbstractPromise::OnPrerequisiteCancelled() {
  switch (GetPrerequisitePolicy()) {
    case Executor::PrerequisitePolicy::kAll:
      // PrerequisitePolicy::kAll should cancel immediately.
      OnCanceled();
      return false;

    case Executor::PrerequisitePolicy::kAny:
      // PrerequisitePolicy::kAny should only cancel if all if it's
      // pre-requisites have been canceled.
      if (prerequisites_->DecrementPrerequisiteCountAndCheckIfZero()) {
        OnCanceled();
        return false;
      }
      return true;

    case Executor::PrerequisitePolicy::kNever:
      // If we we where resolved with a promise then we can't have had
      // PrerequisitePolicy::kAny or PrerequisitePolicy::kNever before the
      // executor was replaced with the curried promise, so pass on
      // cancellation.
      if (IsResolvedWithPromise())
        OnCanceled();
      return false;
  }
}

void AbstractPromise::OnResolvePostReadyDependents() {
  DependentList::Node* dependent_list = dependents_.ConsumeOnceForResolve();
  dependent_list = NonThreadSafeReverseList(dependent_list);

#if DCHECK_IS_ON()
  PassCatchResponsibilityOntoDependentsForCurriedPromise(dependent_list);
#endif

  // Propagate resolve to dependents.
  DependentList::Node* next;
  for (DependentList::Node* node = dependent_list; node; node = next) {
    // We want to release |node->dependent| but we need to do so before
    // we post a task to execute |dependent| on what might be another thread.
    scoped_refptr<AbstractPromise> dependent = std::move(node->dependent);
    // OnPrerequisiteResolved might post a task which destructs |node| on
    // another thread so load |node->next| now.
    next = node->next.load(std::memory_order_relaxed);
    dependent->OnPrerequisiteResolved();
  }
}

void AbstractPromise::OnRejectPostReadyDependents() {
  DependentList::Node* dependent_list = dependents_.ConsumeOnceForReject();
  dependent_list = NonThreadSafeReverseList(dependent_list);

#if DCHECK_IS_ON()
  PassCatchResponsibilityOntoDependentsForCurriedPromise(dependent_list);
#endif

  // Propagate rejection to dependents. We always propagate rejection
  // immediately.
  DependentList::Node* next;
  for (DependentList::Node* node = dependent_list; node; node = next) {
    // We want to release |node->dependent| but we need to do so before
    // we post a task to execute |dependent| on what might be another thread.
    scoped_refptr<AbstractPromise> dependent = std::move(node->dependent);
    // OnPrerequisiteRejected might post a task which destructs |node| on
    // another thread so load |node->next| now.
    next = node->next.load(std::memory_order_relaxed);
    dependent->OnPrerequisiteRejected();
  }
}

void AbstractPromise::OnCanceled() {
  if (dependents_.IsCanceled() || dependents_.IsResolved() ||
      dependents_.IsRejected()) {
    return;
  }

#if DCHECK_IS_ON()
  {
    CheckedAutoLock lock(GetCheckedLock());
    passed_catch_responsibility_ = true;
  }
#endif

  DependentList::Node* dependent_list = dependents_.ConsumeOnceForCancel();

  // Release all pre-requisites to prevent memory leaks
  if (prerequisites_) {
    for (AdjacencyListNode& node : prerequisites_->prerequisite_list) {
      node.prerequisite = nullptr;
    }
  }

  // Propagate cancellation to dependents.
  while (dependent_list) {
    scoped_refptr<AbstractPromise> dependent =
        std::move(dependent_list->dependent);
    dependent->OnPrerequisiteCancelled();
    dependent_list = dependent_list->next.load(std::memory_order_relaxed);
  }
}

void AbstractPromise::OnResolved() {
#if DCHECK_IS_ON()
  DCHECK(executor_can_resolve_) << from_here_.ToString();
#endif
  if (IsResolvedWithPromise()) {
    scoped_refptr<AbstractPromise> curried_promise =
        unique_any_cast<scoped_refptr<AbstractPromise>>(value_);

    // This only happens for PostTasks that returned a promise.
    if (!task_runner_)
      task_runner_ = SequencedTaskRunnerHandle::Get();

    if (!curried_promise->prerequisites_)
      curried_promise->prerequisites_ = std::make_unique<AdjacencyList>();

      // Throw away any existing dependencies and make |curried_promise| the
      // only dependency of this promise.
#if DCHECK_IS_ON()
    {
      CheckedAutoLock lock(GetCheckedLock());
      ancestor_that_could_resolve_ = nullptr;
      ancestor_that_could_reject_ = nullptr;
    }
#endif
    prerequisites_->ResetWithSingleDependency(curried_promise);
    AddAsDependentForAllPrerequisites();
  } else {
    OnResolvePostReadyDependents();
  }
}

void AbstractPromise::OnRejected() {
#if DCHECK_IS_ON()
  DCHECK(executor_can_reject_) << from_here_.ToString();
#endif
  OnRejectPostReadyDependents();
}

// static
DependentList::Node* AbstractPromise::NonThreadSafeReverseList(
    DependentList::Node* list) {
  DependentList::Node* prev = nullptr;
  while (list) {
    DependentList::Node* next = list->next.load(std::memory_order_relaxed);
    list->next.store(prev, std::memory_order_relaxed);
    prev = list;
    list = next;
  }
  return prev;
}

AbstractPromise::AdjacencyListNode::AdjacencyListNode() = default;

AbstractPromise::AdjacencyListNode::AdjacencyListNode(
    scoped_refptr<AbstractPromise> promise)
    : prerequisite(std::move(promise)) {}

AbstractPromise::AdjacencyListNode::~AdjacencyListNode() = default;

AbstractPromise::AdjacencyListNode::AdjacencyListNode(
    AdjacencyListNode&& other) noexcept = default;

AbstractPromise::AdjacencyList::AdjacencyList() = default;

AbstractPromise::AdjacencyList::AdjacencyList(
    scoped_refptr<AbstractPromise> prerequisite)
    : prerequisite_list(1), action_prerequisite_count(1) {
  prerequisite_list[0].prerequisite = std::move(prerequisite);
}

AbstractPromise::AdjacencyList::AdjacencyList(
    std::vector<AdjacencyListNode> nodes)
    : prerequisite_list(std::move(nodes)),
      action_prerequisite_count(prerequisite_list.size()) {}

AbstractPromise::AdjacencyList::~AdjacencyList() = default;

bool AbstractPromise::AdjacencyList::
    DecrementPrerequisiteCountAndCheckIfZero() {
  return action_prerequisite_count.fetch_sub(1, std::memory_order_acq_rel) == 1;
}

void AbstractPromise::AdjacencyList::ResetWithSingleDependency(
    scoped_refptr<AbstractPromise> prerequisite) {
  prerequisite_list.clear();
  prerequisite_list.push_back(AdjacencyListNode{std::move(prerequisite)});
  action_prerequisite_count = 1;
}

}  // namespace internal
}  // namespace base
