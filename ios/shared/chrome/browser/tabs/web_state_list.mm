// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/tabs/web_state_list.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/shared/chrome/browser/tabs/web_state_list_observer.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Wrapper around a WebState stored in a WebStateList. May own the WebState
// dependending on the WebStateList ownership setting (should always be true
// once ownership of Tab is sane, see http://crbug.com/546222 for progress).
class WebStateList::WebStateWrapper {
 public:
  WebStateWrapper(web::WebState* web_state, bool assume_ownership);
  ~WebStateWrapper();

  web::WebState* web_state() const { return web_state_; }
  void set_web_state(web::WebState* web_state) { web_state_ = web_state; }

 private:
  web::WebState* web_state_;
  const bool has_web_state_ownership_;

  DISALLOW_COPY_AND_ASSIGN(WebStateWrapper);
};

WebStateList::WebStateWrapper::WebStateWrapper(web::WebState* web_state,
                                               bool assume_ownership)
    : web_state_(web_state), has_web_state_ownership_(assume_ownership) {}

WebStateList::WebStateWrapper::~WebStateWrapper() {
  if (has_web_state_ownership_)
    delete web_state_;
}

WebStateList::WebStateList(WebStateOwnership ownership)
    : web_state_ownership_(ownership) {}

WebStateList::~WebStateList() = default;

bool WebStateList::ContainsIndex(int index) const {
  return 0 <= index && index < count();
}

web::WebState* WebStateList::GetWebStateAt(int index) const {
  DCHECK(ContainsIndex(index));
  return web_state_wrappers_[index]->web_state();
}

int WebStateList::GetIndexOfWebState(const web::WebState* web_state) const {
  for (int index = 0; index < count(); ++index) {
    if (web_state_wrappers_[index]->web_state() == web_state)
      return index;
  }
  return kInvalidIndex;
}

void WebStateList::InsertWebState(int index, web::WebState* web_state) {
  DCHECK(ContainsIndex(index) || index == count());
  web_state_wrappers_.insert(
      web_state_wrappers_.begin() + index,
      base::MakeUnique<WebStateWrapper>(web_state,
                                        web_state_ownership_ == WebStateOwned));

  for (auto& observer : observers_)
    observer.WebStateInsertedAt(this, web_state, index);
}

void WebStateList::MoveWebStateAt(int from_index, int to_index) {
  DCHECK(ContainsIndex(from_index));
  DCHECK(ContainsIndex(to_index));
  if (from_index == to_index)
    return;

  std::unique_ptr<WebStateWrapper> web_state_wrapper =
      std::move(web_state_wrappers_[from_index]);
  web::WebState* web_state = web_state_wrapper->web_state();
  web_state_wrappers_.erase(web_state_wrappers_.begin() + from_index);
  web_state_wrappers_.insert(web_state_wrappers_.begin() + to_index,
                             std::move(web_state_wrapper));

  for (auto& observer : observers_)
    observer.WebStateMoved(this, web_state, from_index, to_index);
}

web::WebState* WebStateList::ReplaceWebStateAt(int index,
                                               web::WebState* web_state) {
  DCHECK(ContainsIndex(index));
  web::WebState* old_web_state = web_state_wrappers_[index]->web_state();
  web_state_wrappers_[index]->set_web_state(web_state);

  for (auto& observer : observers_)
    observer.WebStateReplacedAt(this, old_web_state, web_state, index);

  return old_web_state;
}

void WebStateList::DetachWebStateAt(int index) {
  DCHECK(ContainsIndex(index));
  web::WebState* web_state = web_state_wrappers_[index]->web_state();
  web_state_wrappers_.erase(web_state_wrappers_.begin() + index);

  for (auto& observer : observers_)
    observer.WebStateDetachedAt(this, web_state, index);
}

void WebStateList::AddObserver(WebStateListObserver* observer) {
  observers_.AddObserver(observer);
}

void WebStateList::RemoveObserver(WebStateListObserver* observer) {
  observers_.RemoveObserver(observer);
}

// static
const int WebStateList::kInvalidIndex;
