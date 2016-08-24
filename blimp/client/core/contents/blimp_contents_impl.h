// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_IMPL_H_
#define BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_IMPL_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "blimp/client/core/contents/blimp_navigation_controller_delegate.h"
#include "blimp/client/core/contents/blimp_navigation_controller_impl.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif  // defined(OS_ANDROID)

namespace blimp {
namespace client {

#if defined(OS_ANDROID)
class BlimpContentsImplAndroid;
#endif  // defined(OS_ANDROID)

class BlimpContentsObserver;
class BlimpNavigationController;
class TabControlFeature;

class BlimpContentsImpl : public BlimpContents,
                          public BlimpNavigationControllerDelegate {
 public:
  explicit BlimpContentsImpl(int id, TabControlFeature* tab_control_feature);
  ~BlimpContentsImpl() override;

#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
  BlimpContentsImplAndroid* GetBlimpContentsImplAndroid();
#endif  // defined(OS_ANDROID)

  // BlimpContents implementation.
  BlimpNavigationControllerImpl& GetNavigationController() override;
  void AddObserver(BlimpContentsObserver* observer) override;
  void RemoveObserver(BlimpContentsObserver* observer) override;

  // Check if some observer is in the observer list.
  bool HasObserver(BlimpContentsObserver* observer);

  // BlimpNavigationControllerDelegate implementation.
  void OnNavigationStateChanged() override;

  // Pushes the size and scale information to the engine, which will affect the
  // web content display area for all tabs.
  void SetSizeAndScale(const gfx::Size& size, float device_pixel_ratio);

  int id() { return id_; }

 private:
  // Handles the back/forward list and loading URLs.
  BlimpNavigationControllerImpl navigation_controller_;

  // A list of all the observers of this BlimpContentsImpl.
  base::ObserverList<BlimpContentsObserver> observers_;

  // The id is assigned during contents creation. It is used by
  // BlimpContentsManager to control the life time of the its observer.
  int id_;

  // The tab control feature through which the BlimpContentsImpl is able to
  // set size and scale.
  // TODO(mlliu): in the long term, we want to put size and scale in a different
  // feature instead of tab control feature. crbug.com/639154.
  TabControlFeature* tab_control_feature_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentsImpl);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_IMPL_H_
