// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_LIFECYCLE_AW_CONTENTS_LIFECYCLE_NOTIFIER_H_
#define ANDROID_WEBVIEW_BROWSER_LIFECYCLE_AW_CONTENTS_LIFECYCLE_NOTIFIER_H_

#include <map>

#include "android_webview/browser/lifecycle/webview_app_state_observer.h"
#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"

namespace android_webview {

class AwContents;

class AwContentsLifecycleNotifier {
 public:
  enum class AwContentsState {
    // AwContents isn't attached to a window.
    kDetached,
    // AwContents is attached to a window and window is visible.
    kForeground,
    // AwContents is attached to a window and window is invisible.
    kBackground,
  };

  static AwContentsLifecycleNotifier& GetInstance();

  void OnWebViewCreated(const AwContents* aw_contents);
  void OnWebViewDestroyed(const AwContents* aw_contents);
  void OnWebViewAttachedToWindow(const AwContents* aw_contents);
  void OnWebViewDetachedFromWindow(const AwContents* aw_contents);
  void OnWebViewWindowBeVisible(const AwContents* aw_contents);
  void OnWebViewWindowBeInvisible(const AwContents* aw_contents);

  void AddObserver(WebViewAppStateObserver* observer);
  void RemoveObserver(WebViewAppStateObserver* observer);

  bool has_aw_contents_ever_created() const {
    return has_aw_contents_ever_created_;
  }

  std::vector<const AwContents*> GetAllAwContents() const;

 private:
  struct AwContentsData {
    AwContentsData();
    AwContentsData(AwContentsData&& data);
    ~AwContentsData();

    bool attached_to_window = false;
    bool window_visible = false;
    AwContentsState aw_content_state = AwContentsState::kDetached;

   private:
    DISALLOW_COPY(AwContentsData);
  };

  friend base::NoDestructor<AwContentsLifecycleNotifier>;
  friend class TestAwContentsLifecycleNotifier;

  AwContentsLifecycleNotifier();
  virtual ~AwContentsLifecycleNotifier();

  size_t ToIndex(AwContentsState state) const;
  void OnAwContentsStateChanged(
      AwContentsLifecycleNotifier::AwContentsData* data);

  void UpdateAppState();

  bool HasAwContentsInstance() const;

  AwContentsLifecycleNotifier::AwContentsData* GetAwContentsData(
      const AwContents* aw_contents);

  // The AwContents to AwContentsData mapping.
  std::map<const AwContents*, AwContentsLifecycleNotifier::AwContentsData>
      aw_contents_to_data_;

  // The number of AwContents instances in each AwContentsState.
  int state_count_[3]{};

  bool has_aw_contents_ever_created_ = false;

  base::ObserverList<WebViewAppStateObserver>::Unchecked observers_;
  WebViewAppStateObserver::State app_state_ =
      WebViewAppStateObserver::State::kDestroyed;

  DISALLOW_COPY_AND_ASSIGN(AwContentsLifecycleNotifier);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_LIFECYCLE_AW_CONTENTS_LIFECYCLE_NOTIFIER_H_
