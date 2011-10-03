// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper class used to find the RenderViewObservers for a given RenderView.
//
// Example usage:
//
//   class MyRVO : public RenderViewObserver,
//                 public RenderViewObserverTracker<MyRVO> {
//     ...
//   };
//
//   MyRVO::MyRVO(RenderView* render_view)
//       : RenderViewObserver(render_view),
//         RenderViewObserverTracker<MyRVO>(render_view) {
//     ...
//   }
//
//  void SomeFunction(RenderView* rv) {
//    MyRVO* my_rvo = new MyRVO(rv);
//    MyRVO* my_rvo_tracked = MyRVO::Get(rv);
//    // my_rvo == my_rvo_tracked
//  }

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_VIEW_OBSERVER_TRACKER_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_VIEW_OBSERVER_TRACKER_H_
#pragma once

#include <map>

#include "base/lazy_instance.h"

class RenderView;

namespace content {

template <class T>
class RenderViewObserverTracker {
 public:
  static T* Get(const RenderView* render_view) {
    return render_view_map_.Get()[render_view];
  }

  explicit RenderViewObserverTracker(const RenderView* render_view)
      : render_view_(render_view) {
    render_view_map_.Get()[render_view] = static_cast<T*>(this);
  }
  ~RenderViewObserverTracker() {
    render_view_map_.Get().erase(render_view_);
  }

 private:
  const RenderView* render_view_;

  static base::LazyInstance<std::map<const RenderView*, T*> >
    render_view_map_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewObserverTracker<T>);
};

template <class T>
base::LazyInstance<std::map<const RenderView*, T*> >
    RenderViewObserverTracker<T>::render_view_map_(base::LINKER_INITIALIZED);

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_VIEW_OBSERVER_TRACKER_H_
