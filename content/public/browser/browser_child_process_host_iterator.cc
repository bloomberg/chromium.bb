// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_child_process_host_iterator.h"

#include "base/logging.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

BrowserChildProcessHostIterator::BrowserChildProcessHostIterator()
    : all_(true), type_(content::PROCESS_TYPE_UNKNOWN) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO)) <<
        "BrowserChildProcessHostIterator must be used on the IO thread.";
  iterator_ = BrowserChildProcessHostImpl::GetIterator()->begin();
}

BrowserChildProcessHostIterator::BrowserChildProcessHostIterator(
    content::ProcessType type)
    : all_(false), type_(type) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO)) <<
        "BrowserChildProcessHostIterator must be used on the IO thread.";
  iterator_ = BrowserChildProcessHostImpl::GetIterator()->begin();
  if (!Done() && (*iterator_)->GetData().type != type_)
    ++(*this);
}

bool BrowserChildProcessHostIterator::operator++() {
  CHECK(!Done());
  do {
    ++iterator_;
    if (Done())
      break;

    if (!all_ && (*iterator_)->GetData().type != type_)
      continue;

    return true;
  } while (true);

  return false;
}

bool BrowserChildProcessHostIterator::Done() {
  return iterator_ == BrowserChildProcessHostImpl::GetIterator()->end();
}

const ChildProcessData& BrowserChildProcessHostIterator::GetData() {
  CHECK(!Done());
  return (*iterator_)->GetData();
}

bool BrowserChildProcessHostIterator::Send(IPC::Message* message) {
  CHECK(!Done());
  return (*iterator_)->Send(message);
}

BrowserChildProcessHostDelegate*
    BrowserChildProcessHostIterator::GetDelegate() {
  return (*iterator_)->delegate();
}

}  // namespace content
