// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/batch_element_checker.h"

#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/web_controller.h"

namespace autofill_assistant {
namespace {
// Waiting period between two checks.
static constexpr base::TimeDelta kCheckPeriod =
    base::TimeDelta::FromMilliseconds(100);
}  // namespace

BatchElementChecker::BatchElementChecker(WebController* web_controller)
    : web_controller_(web_controller),
      all_found_(false),
      stopped_(false),
      weak_ptr_factory_(this) {
  DCHECK(web_controller);
}

BatchElementChecker::~BatchElementChecker() {}

void BatchElementChecker::AddElementExistenceCheck(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(!try_done_callback_);

  element_callback_map_[selectors].element_exists_callbacks.emplace_back(
      std::move(callback));
}

void BatchElementChecker::AddFieldValueCheck(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool, const std::string&)> callback) {
  DCHECK(!try_done_callback_);

  element_callback_map_[selectors].get_field_value_callbacks.emplace_back(
      std::move(callback));
}

void BatchElementChecker::Run(const base::TimeDelta& duration,
                              base::RepeatingCallback<void()> try_done,
                              base::OnceCallback<void()> all_done) {
  int64_t try_count = duration / kCheckPeriod;
  if (try_count <= 0) {
    try_count = 1;
  }

  Try(base::BindOnce(
      &BatchElementChecker::OnTryDone,
      // Callback is run from this class, so this is guaranteed to still exist.
      base::Unretained(this), try_count, try_done, std::move(all_done)));
}

BatchElementChecker::ElementCallbacks::ElementCallbacks() {}
BatchElementChecker::ElementCallbacks::~ElementCallbacks() {}

void BatchElementChecker::Try(base::OnceCallback<void()> try_done_callback) {
  DCHECK(!try_done_callback_);

  if (stopped_) {
    std::move(try_done_callback).Run();
    return;
  }
  try_done_callback_ = std::move(try_done_callback);
  RunSequentially(element_callback_map_.begin());
}

void BatchElementChecker::OnTryDone(int64_t remaining_attempts,
                                    base::RepeatingCallback<void()> try_done,
                                    base::OnceCallback<void()> all_done) {
  if (all_found_) {
    try_done.Run();
    std::move(all_done).Run();
    return;
  }

  --remaining_attempts;
  if (remaining_attempts <= 0 || stopped_) {
    // GiveUp is run before calling try_done, so its effects are visible right
    // away.
    GiveUp();
    try_done.Run();
    std::move(all_done).Run();
    return;
  }
  try_done.Run();

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &BatchElementChecker::Try, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&BatchElementChecker::OnTryDone,
                         weak_ptr_factory_.GetWeakPtr(), remaining_attempts,
                         try_done, std::move(all_done))),
      kCheckPeriod);
}

void BatchElementChecker::GiveUp() {
  for (auto& entry : element_callback_map_) {
    ElementCallbacks* callbacks = &entry.second;
    RunElementExistsCallbacks(callbacks, false);
    RunGetFieldValueCallbacks(callbacks, false, "");
  }
  element_callback_map_.clear();
}

void BatchElementChecker::RunSequentially(ElementCallbackMap::iterator iter) {
  // Skip uninteresting entries
  while (iter != element_callback_map_.end() && !HasChecksToRun(iter->second)) {
    ++iter;
  }

  // Call try_done_callback once there's nothing left.
  if (iter == element_callback_map_.end()) {
    DCHECK(try_done_callback_);
    all_found_ = !HasMoreChecksToRun();
    std::move(try_done_callback_).Run();
    return;
  }

  if (!iter->second.get_field_value_callbacks.empty()) {
    web_controller_->GetFieldValue(
        iter->first, base::BindOnce(&BatchElementChecker::OnGetFieldValue,
                                    weak_ptr_factory_.GetWeakPtr(), iter));
    return;
  }

  DCHECK(!iter->second.element_exists_callbacks.empty());

  web_controller_->ElementExists(
      iter->first, base::BindOnce(&BatchElementChecker::OnElementExists,
                                  weak_ptr_factory_.GetWeakPtr(), iter));
}

void BatchElementChecker::OnElementExists(ElementCallbackMap::iterator iter,
                                          bool exists) {
  if (exists)
    RunElementExistsCallbacks(&iter->second, true);

  RunSequentially(++iter);
}

void BatchElementChecker::OnGetFieldValue(ElementCallbackMap::iterator iter,
                                          bool exists,
                                          const std::string& value) {
  if (exists) {
    RunElementExistsCallbacks(&iter->second, exists);
    RunGetFieldValueCallbacks(&iter->second, exists, value);
  }

  RunSequentially(++iter);
}

void BatchElementChecker::RunElementExistsCallbacks(ElementCallbacks* callbacks,
                                                    bool result) {
  for (auto& callback : callbacks->element_exists_callbacks) {
    std::move(callback).Run(result);
  }
  callbacks->element_exists_callbacks.clear();
}

void BatchElementChecker::RunGetFieldValueCallbacks(ElementCallbacks* callbacks,
                                                    bool exists,
                                                    const std::string& value) {
  for (auto& callback : callbacks->get_field_value_callbacks) {
    std::move(callback).Run(exists, value);
  }
  callbacks->get_field_value_callbacks.clear();
}

bool BatchElementChecker::HasChecksToRun(const ElementCallbacks& element) {
  if (!element.element_exists_callbacks.empty())
    return true;

  if (!element.get_field_value_callbacks.empty())
    return true;

  return false;
}

bool BatchElementChecker::HasMoreChecksToRun() {
  for (const auto& entry : element_callback_map_) {
    if (HasChecksToRun(entry.second))
      return true;
  }
  return false;
}

}  // namespace autofill_assistant
