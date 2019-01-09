// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_annotation/core/page_annotator.h"

namespace page_image_annotation {

PageAnnotator::Observer::~Observer() {}

PageAnnotator::Subscription::Subscription(
    const Observer* const observer,
    base::WeakPtr<PageAnnotator> page_annotator)
    : observer_(observer), page_annotator_(page_annotator) {}

PageAnnotator::Subscription::Subscription(Subscription&& subscription) =
    default;

PageAnnotator::Subscription::~Subscription() {
  Cancel();
}

void PageAnnotator::Subscription::Cancel() {
  if (page_annotator_)
    page_annotator_->RemoveObserver(observer_);
}

PageAnnotator::PageAnnotator() : weak_ptr_factory_(this) {}

PageAnnotator::~PageAnnotator() {}

void PageAnnotator::ImageAdded(const uint64_t node_id,
                               const std::string& source_id) {
  // TODO(crbug.com/916363): create a connection to the image annotation service
  //                         for this image.
  for (Observer& observer : observers_) {
    observer.OnImageAdded(node_id);
  }
}

void PageAnnotator::ImageModified(const uint64_t node_id,
                                  const std::string& source_id) {
  // TODO(crbug.com/916363): reset the service connection for this image.

  for (Observer& observer : observers_) {
    observer.OnImageModified(node_id);
  }
}

void PageAnnotator::ImageRemoved(const uint64_t node_id) {
  // TODO(crbug.com/916363): close the service connection for this image.
  for (Observer& observer : observers_) {
    observer.OnImageRemoved(node_id);
  }
}

PageAnnotator::Subscription PageAnnotator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  return Subscription(observer, weak_ptr_factory_.GetWeakPtr());
}

void PageAnnotator::RemoveObserver(const Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace page_image_annotation
