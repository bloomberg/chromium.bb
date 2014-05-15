// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/filter_chain.h"

#include <algorithm>

namespace mojo {
namespace internal {

FilterChain::FilterChain(MessageReceiver* sink) : sink_(sink) {
}

FilterChain::FilterChain(RValue other) : sink_(other.object->sink_) {
  other.object->sink_ = NULL;
  filters_.swap(other.object->filters_);
}

FilterChain& FilterChain::operator=(RValue other) {
  std::swap(sink_, other.object->sink_);
  filters_.swap(other.object->filters_);
  return *this;
}

FilterChain::~FilterChain() {
  for (std::vector<MessageFilter*>::iterator iter = filters_.begin();
       iter != filters_.end();
       ++iter) {
    delete *iter;
  }
}

FilterChain& FilterChain::Append(MessageFilter* filter) {
  if (!filters_.empty())
    filters_.back()->set_sink(filter);
  filters_.push_back(filter);

  return *this;
}

FilterChain& FilterChain::Append(PassThroughFilter* filter) {
  delete filter;
  return *this;
}

MessageReceiver* FilterChain::GetHead() {
  assert(sink_);

  if (filters_.empty())
    return sink_;

  filters_.back()->set_sink(sink_);
  return filters_.front();
}

}  // namespace internal
}  // namespace mojo
