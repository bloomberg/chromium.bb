// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_cookie_store_wrapper.h"

#include <string>

#include "android_webview/browser/net/init_native_callback.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/threading/thread_task_runner_handle.h"
#include "url/gurl.h"

namespace android_webview {

namespace {

// Posts |task| to the thread that the global CookieStore lives on.
void PostTaskToCookieStoreTaskRunner(base::OnceClosure task) {
  GetCookieStoreTaskRunner()->PostTask(FROM_HERE, std::move(task));
}

class AwCookieChangedSubscription
    : public net::CookieStore::CookieChangedSubscription {
 public:
  explicit AwCookieChangedSubscription(
      std::unique_ptr<net::CookieStore::CookieChangedCallbackList::Subscription>
          subscription)
      : subscription_(std::move(subscription)) {}

 private:
  std::unique_ptr<net::CookieStore::CookieChangedCallbackList::Subscription>
      subscription_;

  DISALLOW_COPY_AND_ASSIGN(AwCookieChangedSubscription);
};

// Wraps a subscription to cookie change notifications for the global
// CookieStore for a consumer that lives on another thread. Handles passing
// messages between thread, and destroys itself when the consumer unsubscribes.
// Must be created on the consumer's thread. Each instance only supports a
// single subscription.
class SubscriptionWrapper {
 public:
  SubscriptionWrapper() : weak_factory_(this) {}

  std::unique_ptr<net::CookieStore::CookieChangedSubscription> Subscribe(
      const GURL& url,
      const std::string& name,
      const net::CookieStore::CookieChangedCallback& callback) {
    // This class is only intended to be used for a single subscription.
    DCHECK(callback_list_.empty());

    nested_subscription_ =
        new NestedSubscription(url, name, weak_factory_.GetWeakPtr());
    return std::make_unique<AwCookieChangedSubscription>(
        callback_list_.Add(callback));
  }

 private:
  // The NestedSubscription is responsible for creating and managing the
  // underlying subscription to the real CookieStore, and posting notifications
  // back to |callback_list_|.
  class NestedSubscription
      : public base::RefCountedDeleteOnSequence<NestedSubscription> {
   public:
    NestedSubscription(const GURL& url,
                       const std::string& name,
                       base::WeakPtr<SubscriptionWrapper> subscription_wrapper)
        : base::RefCountedDeleteOnSequence<NestedSubscription>(
              GetCookieStoreTaskRunner()),
          subscription_wrapper_(subscription_wrapper),
          client_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
      PostTaskToCookieStoreTaskRunner(
          base::Bind(&NestedSubscription::Subscribe, this, url, name));
    }

   private:
    friend class base::RefCountedDeleteOnSequence<NestedSubscription>;
    friend class base::DeleteHelper<NestedSubscription>;

    ~NestedSubscription() {}

    void Subscribe(const GURL& url, const std::string& name) {
      GetCookieStore()->AddCallbackForCookie(
          url, name, base::Bind(&NestedSubscription::OnChanged, this));
    }

    void OnChanged(const net::CanonicalCookie& cookie,
                   net::CookieStore::ChangeCause cause) {
      client_task_runner_->PostTask(
          FROM_HERE, base::Bind(&SubscriptionWrapper::OnChanged,
                                subscription_wrapper_, cookie, cause));
    }

    base::WeakPtr<SubscriptionWrapper> subscription_wrapper_;
    scoped_refptr<base::TaskRunner> client_task_runner_;

    std::unique_ptr<net::CookieStore::CookieChangedSubscription> subscription_;

    DISALLOW_COPY_AND_ASSIGN(NestedSubscription);
  };

  void OnChanged(const net::CanonicalCookie& cookie,
                 net::CookieStore::ChangeCause cause) {
    callback_list_.Notify(cookie, cause);
  }

  // The "list" only had one entry, so can just clean up now.
  void OnUnsubscribe() { delete this; }

  scoped_refptr<NestedSubscription> nested_subscription_;
  net::CookieStore::CookieChangedCallbackList callback_list_;
  base::WeakPtrFactory<SubscriptionWrapper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SubscriptionWrapper);
};

void SetCookieWithOptionsAsyncOnCookieThread(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    net::CookieStore::SetCookiesCallback callback) {
  GetCookieStore()->SetCookieWithOptionsAsync(url, cookie_line, options,
                                              std::move(callback));
}

void SetCanonicalCookieAsyncOnCookieThread(
    std::unique_ptr<net::CanonicalCookie> cookie,
    bool secure_source,
    bool modify_http_only,
    net::CookieStore::SetCookiesCallback callback) {
  GetCookieStore()->SetCanonicalCookieAsync(
      std::move(cookie), secure_source, modify_http_only, std::move(callback));
}

void GetCookiesWithOptionsAsyncOnCookieThread(
    const GURL& url,
    const net::CookieOptions& options,
    net::CookieStore::GetCookiesCallback callback) {
  GetCookieStore()->GetCookiesWithOptionsAsync(url, options,
                                               std::move(callback));
}

void GetCookieListWithOptionsAsyncOnCookieThread(
    const GURL& url,
    const net::CookieOptions& options,
    net::CookieStore::GetCookieListCallback callback) {
  GetCookieStore()->GetCookieListWithOptionsAsync(url, options,
                                                  std::move(callback));
}

void GetAllCookiesAsyncOnCookieThread(
    net::CookieStore::GetCookieListCallback callback) {
  GetCookieStore()->GetAllCookiesAsync(std::move(callback));
}

void DeleteCookieAsyncOnCookieThread(const GURL& url,
                                     const std::string& cookie_name,
                                     base::OnceClosure callback) {
  GetCookieStore()->DeleteCookieAsync(url, cookie_name, std::move(callback));
}

void DeleteCanonicalCookieAsyncOnCookieThread(
    const net::CanonicalCookie& cookie,
    net::CookieStore::DeleteCallback callback) {
  GetCookieStore()->DeleteCanonicalCookieAsync(cookie, std::move(callback));
}

void DeleteAllCreatedBetweenAsyncOnCookieThread(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    net::CookieStore::DeleteCallback callback) {
  GetCookieStore()->DeleteAllCreatedBetweenAsync(delete_begin, delete_end,
                                                 std::move(callback));
}

void DeleteAllCreatedBetweenWithPredicateAsyncOnCookieThread(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const net::CookieStore::CookiePredicate& predicate,
    net::CookieStore::DeleteCallback callback) {
  GetCookieStore()->DeleteAllCreatedBetweenWithPredicateAsync(
      delete_begin, delete_end, predicate, std::move(callback));
}

void DeleteSessionCookiesAsyncOnCookieThread(
    net::CookieStore::DeleteCallback callback) {
  GetCookieStore()->DeleteSessionCookiesAsync(std::move(callback));
}

void FlushStoreOnCookieThread(base::OnceClosure callback) {
  GetCookieStore()->FlushStore(std::move(callback));
}

void SetForceKeepSessionStateOnCookieThread() {
  GetCookieStore()->SetForceKeepSessionState();
}

}  // namespace

AwCookieStoreWrapper::AwCookieStoreWrapper()
    : client_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {}

AwCookieStoreWrapper::~AwCookieStoreWrapper() {}

void AwCookieStoreWrapper::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    net::CookieStore::SetCookiesCallback callback) {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
  PostTaskToCookieStoreTaskRunner(base::BindOnce(
      &SetCookieWithOptionsAsyncOnCookieThread, url, cookie_line, options,
      CreateWrappedCallback<bool>(std::move(callback))));
}

void AwCookieStoreWrapper::SetCanonicalCookieAsync(
    std::unique_ptr<net::CanonicalCookie> cookie,
    bool secure_source,
    bool modify_http_only,
    SetCookiesCallback callback) {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
  PostTaskToCookieStoreTaskRunner(base::BindOnce(
      &SetCanonicalCookieAsyncOnCookieThread, std::move(cookie), secure_source,
      modify_http_only, CreateWrappedCallback<bool>(std::move(callback))));
}

void AwCookieStoreWrapper::GetCookiesWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookiesCallback callback) {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
  PostTaskToCookieStoreTaskRunner(base::BindOnce(
      &GetCookiesWithOptionsAsyncOnCookieThread, url, options,
      CreateWrappedCallback<const std::string&>(std::move(callback))));
}

void AwCookieStoreWrapper::GetCookieListWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookieListCallback callback) {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
  PostTaskToCookieStoreTaskRunner(base::BindOnce(
      &GetCookieListWithOptionsAsyncOnCookieThread, url, options,
      CreateWrappedCallback<const net::CookieList&>(std::move(callback))));
}

void AwCookieStoreWrapper::GetAllCookiesAsync(GetCookieListCallback callback) {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
  PostTaskToCookieStoreTaskRunner(base::BindOnce(
      &GetAllCookiesAsyncOnCookieThread,
      CreateWrappedCallback<const net::CookieList&>(std::move(callback))));
}

void AwCookieStoreWrapper::DeleteCookieAsync(const GURL& url,
                                             const std::string& cookie_name,
                                             base::OnceClosure callback) {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
  PostTaskToCookieStoreTaskRunner(
      base::BindOnce(&DeleteCookieAsyncOnCookieThread, url, cookie_name,
                     CreateWrappedClosureCallback(std::move(callback))));
}

void AwCookieStoreWrapper::DeleteCanonicalCookieAsync(
    const net::CanonicalCookie& cookie,
    DeleteCallback callback) {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
  PostTaskToCookieStoreTaskRunner(
      base::BindOnce(&DeleteCanonicalCookieAsyncOnCookieThread, cookie,
                     CreateWrappedCallback<uint32_t>(std::move(callback))));
}

void AwCookieStoreWrapper::DeleteAllCreatedBetweenAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    DeleteCallback callback) {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
  PostTaskToCookieStoreTaskRunner(base::BindOnce(
      &DeleteAllCreatedBetweenAsyncOnCookieThread, delete_begin, delete_end,
      CreateWrappedCallback<uint32_t>(std::move(callback))));
}

void AwCookieStoreWrapper::DeleteAllCreatedBetweenWithPredicateAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const CookiePredicate& predicate,
    DeleteCallback callback) {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
  PostTaskToCookieStoreTaskRunner(
      base::BindOnce(&DeleteAllCreatedBetweenWithPredicateAsyncOnCookieThread,
                     delete_begin, delete_end, predicate,
                     CreateWrappedCallback<uint32_t>(std::move(callback))));
}

void AwCookieStoreWrapper::DeleteSessionCookiesAsync(DeleteCallback callback) {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
  PostTaskToCookieStoreTaskRunner(
      base::BindOnce(&DeleteSessionCookiesAsyncOnCookieThread,
                     CreateWrappedCallback<uint32_t>(std::move(callback))));
}

void AwCookieStoreWrapper::FlushStore(base::OnceClosure callback) {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
  PostTaskToCookieStoreTaskRunner(
      base::BindOnce(&FlushStoreOnCookieThread,
                     CreateWrappedClosureCallback(std::move(callback))));
}

void AwCookieStoreWrapper::SetForceKeepSessionState() {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
  PostTaskToCookieStoreTaskRunner(
      base::Bind(&SetForceKeepSessionStateOnCookieThread));
}

std::unique_ptr<net::CookieStore::CookieChangedSubscription>
AwCookieStoreWrapper::AddCallbackForCookie(
    const GURL& url,
    const std::string& name,
    const CookieChangedCallback& callback) {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());

  // The SubscriptionWrapper is owned by the subscription itself, and has no
  // connection to the AwCookieStoreWrapper after creation. Other CookieStore
  // implementations DCHECK if a subscription outlasts the cookie store,
  // unfortunately, this design makes DCHECKing if there's an outstanding
  // subscription when the AwCookieStoreWrapper is destroyed a bit ugly.
  // TODO(mmenke):  Still worth adding a DCHECK?
  SubscriptionWrapper* subscription = new SubscriptionWrapper();
  return subscription->Subscribe(url, name, callback);
}

std::unique_ptr<net::CookieStore::CookieChangedSubscription>
AwCookieStoreWrapper::AddCallbackForAllChanges(
    const CookieChangedCallback& callback) {
  // TODO(rdsmith): Implement when needed by Android Webview consumer.
  CHECK(false);
  return nullptr;
}

bool AwCookieStoreWrapper::IsEphemeral() {
  return GetCookieStore()->IsEphemeral();
}

base::OnceClosure AwCookieStoreWrapper::CreateWrappedClosureCallback(
    base::OnceClosure callback) {
  if (callback.is_null())
    return callback;
  return base::BindOnce(
      base::IgnoreResult(&base::TaskRunner::PostTask), client_task_runner_,
      FROM_HERE,
      base::BindOnce(&AwCookieStoreWrapper::RunClosureCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AwCookieStoreWrapper::RunClosureCallback(base::OnceClosure callback) {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
  std::move(callback).Run();
}

}  // namespace android_webview
