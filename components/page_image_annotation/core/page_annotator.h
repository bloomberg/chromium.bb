// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_IMAGE_ANNOTATION_CORE_PAGE_ANNOTATOR_H_
#define COMPONENTS_PAGE_IMAGE_ANNOTATION_CORE_PAGE_ANNOTATOR_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace page_image_annotation {

// Notifies clients of page images that can be annotated and forwards annotation
// requests for these images to the image annotation service.
//
// TODO(crbug.com/916363): this class is not yet complete - add more logic (e.g.
//                         communication with the service).
class PageAnnotator {
 public:
  // Clients (i.e. classes that annotate page images) should implement this
  // interface.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;

    // These methods are called during page lifecycle to notify the observer
    // about changes to page images.

    // Called exactly once per image, at the point that the image appears on the
    // page (or at the point that the observer subscribes to the page annotator,
    // if the image already exists on page).
    virtual void OnImageAdded(uint64_t node_id) = 0;

    // Called at the point that an image source is updated.
    virtual void OnImageModified(uint64_t node_id) = 0;

    // Called at the point that an image disappears from the page.
    virtual void OnImageRemoved(uint64_t node_id) = 0;
  };

  // A subscription instance must be held by each observer of the page
  // annotator; an observer will receive updates from the page annotator until
  // the Cancel method of the subscription is called (this occurs automatically
  // on subscription destruction).
  //
  // Typically, both the page annotator and its observers are scoped to the
  // lifetime of a render frame. Destruction of such objects can proceed in an
  // unspecified order, so subscriptions are used to ensure the page annotator
  // only communicates with an observers that are still alive.
  class Subscription {
   public:
    Subscription(const Observer* observer,
                 base::WeakPtr<PageAnnotator> page_annotator);
    Subscription(Subscription&& subscription);
    ~Subscription();

    // Unsubscribe from updates from the page annotator.
    void Cancel();

   private:
    const Observer* observer_;
    base::WeakPtr<PageAnnotator> page_annotator_;

    DISALLOW_COPY_AND_ASSIGN(Subscription);
  };

  PageAnnotator();
  ~PageAnnotator();

  // Called by platform drivers.
  void ImageAdded(uint64_t node_id, const std::string& source_id);
  void ImageModified(uint64_t node_id, const std::string& source_id);
  void ImageRemoved(uint64_t node_id);

  Subscription AddObserver(Observer* observer) WARN_UNUSED_RESULT;

 private:
  void RemoveObserver(const Observer* observer);

  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<PageAnnotator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageAnnotator);
};

}  // namespace page_image_annotation

#endif  // COMPONENTS_PAGE_IMAGE_ANNOTATION_CORE_PAGE_ANNOTATOR_H_
