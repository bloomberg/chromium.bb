// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/page_load_tracker_decorator_helper.h"

#include "base/bind.h"
#include "components/performance_manager/decorators/page_load_tracker_decorator.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace performance_manager {

namespace {

void NotifyPageLoadTrackerDecoratorOnPMSequence(content::WebContents* contents,
                                                void (*method)(PageNodeImpl*)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> node, void (*method)(PageNodeImpl*)) {
            if (node) {
              PageNodeImpl* page_node = PageNodeImpl::FromNode(node.get());
              method(page_node);
            }
          },
          PerformanceManager::GetPageNodeForWebContents(contents), method));
}

}  // namespace

// Listens to content::WebContentsObserver notifications for a given WebContents
// and updates the PageLoadTracker accordingly. Destroys itself when the
// WebContents it observes is destroyed.
class PageLoadTrackerDecoratorHelper::WebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit WebContentsObserver(content::WebContents* web_contents,
                               PageLoadTrackerDecoratorHelper* outer)
      : content::WebContentsObserver(web_contents),
        outer_(outer),
        prev_(nullptr),
        next_(outer->first_web_contents_observer_) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(web_contents);

    if (next_) {
      DCHECK(!next_->prev_);
      next_->prev_ = this;
    }
    outer_->first_web_contents_observer_ = this;

    if (web_contents->IsLoadingToDifferentDocument() &&
        !web_contents->IsWaitingForResponse()) {
      // Simulate receiving the missed DidReceiveResponse() notification.
      DidReceiveResponse();
    }
  }

  WebContentsObserver(const WebContentsObserver&) = delete;
  WebContentsObserver& operator=(const WebContentsObserver&) = delete;

  ~WebContentsObserver() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  // content::WebContentsObserver:
  void DidReceiveResponse() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Only observe top-level navigation to a different document.
    if (!web_contents()->IsLoadingToDifferentDocument())
      return;

    DCHECK(web_contents()->IsLoading());

#if DCHECK_IS_ON()
    DCHECK(!did_receive_response_);
    did_receive_response_ = true;
#endif
    NotifyPageLoadTrackerDecoratorOnPMSequence(
        web_contents(), &PageLoadTrackerDecorator::DidReceiveResponse);
  }

  void DidStopLoading() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if DCHECK_IS_ON()
    did_receive_response_ = false;
#endif
    NotifyPageLoadTrackerDecoratorOnPMSequence(
        web_contents(), &PageLoadTrackerDecorator::DidStopLoading);
  }

  void WebContentsDestroyed() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DetachAndDestroy();
  }

  // Removes the WebContentsObserver from the linked list and deletes it.
  void DetachAndDestroy() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (prev_) {
      DCHECK_EQ(prev_->next_, this);
      prev_->next_ = next_;
    } else {
      DCHECK_EQ(outer_->first_web_contents_observer_, this);
      outer_->first_web_contents_observer_ = next_;
    }
    if (next_) {
      DCHECK_EQ(next_->prev_, this);
      next_->prev_ = prev_;
    }

    delete this;
  }

 private:
  // TODO(https://crbug.com/1048719): Extract the logic to manage a linked list
  // of WebContentsObservers to a helper class.
  PageLoadTrackerDecoratorHelper* const outer_;
  WebContentsObserver* prev_;
  WebContentsObserver* next_;

#if DCHECK_IS_ON()
  // Used to verify the invariant that DidReceiveResponse() cannot be called
  // twice in a row without a DidStopLoading() in between.
  bool did_receive_response_ = false;
#endif

  SEQUENCE_CHECKER(sequence_checker_);
};

PageLoadTrackerDecoratorHelper::PageLoadTrackerDecoratorHelper() {
  PerformanceManager::AddObserver(this);
}

PageLoadTrackerDecoratorHelper::~PageLoadTrackerDecoratorHelper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Destroy all WebContentsObserver to ensure that PageLiveStateDecorators are
  // no longer maintained.
  while (first_web_contents_observer_)
    first_web_contents_observer_->DetachAndDestroy();

  PerformanceManager::RemoveObserver(this);
}

void PageLoadTrackerDecoratorHelper::OnPageNodeCreatedForWebContents(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);
  // Start observing the WebContents. See comment on
  // |first_web_contents_observer_| for lifetime management details.
  new WebContentsObserver(web_contents, this);
}

}  // namespace performance_manager
