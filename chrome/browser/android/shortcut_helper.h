// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
#define CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/common/web_application_info.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/manifest.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"

namespace content {
class WebContents;
}  // namespace content

namespace IPC {
class Message;
}

class GURL;

// ShortcutHelper is the C++ counterpart of org.chromium.chrome.browser's
// ShortcutHelper in Java. The object is owned by the Java object. It is created
// from there via a JNI (Initialize) call and can be destroyed from Java too
// using TearDown. When the Java implementations calls AddShortcut, it gives up
// the ownership of the object. The instance will then destroy itself when done.
class ShortcutHelper : public content::WebContentsObserver {
 public:
  ShortcutHelper(JNIEnv* env,
                 jobject obj,
                 content::WebContents* web_contents);

  // Initialize the helper by requesting the information about the page to the
  // renderer process. The initialization is asynchronous and
  // OnDidRetrieveWebappInformation is expected to be called when finished.
  void Initialize();

  // Called by the Java counter part to let the object knows that it can destroy
  // itself.
  void TearDown(JNIEnv* env, jobject obj);

  // IPC message received when the initialization is finished.
  void OnDidGetWebApplicationInfo(const WebApplicationInfo& web_app_info);

  // Callback run when the Manifest is ready to be used.
  void OnDidGetManifest(const content::Manifest& manifest);

  // Adds a shortcut to the current URL to the Android home screen.
  void AddShortcut(JNIEnv* env,
                   jobject obj,
                   jstring title,
                   jint launcher_large_icon_size);

  // Callback run when the requested Manifest icon is ready to be used.
  void OnDidDownloadIcon(int id,
                         int http_status_code,
                         const GURL& url,
                         const std::vector<SkBitmap>& bitmaps,
                         const std::vector<gfx::Size>& sizes);

  // Called after AddShortcut() and OnDidDownloadIcon() are run if
  // OnDidDownloadIcon has a valid icon.
  void AddShortcutUsingManifestIcon();

  // Use FaviconService to get the best available favicon and create the
  // shortcut using it. This is used when no Manifest icons are available or
  // appropriate.
  void AddShortcutUsingFavicon();

  // Callback run when a favicon is received from GetFavicon() call.
  void OnDidGetFavicon(
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // WebContentsObserver
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;

  // Adds a shortcut to the launcher using a FaviconRawBitmapResult.
  // Must be called from a WorkerPool task.
  static void AddShortcutInBackgroundWithRawBitmap(
      const GURL& url,
      const base::string16& title,
      content::Manifest::DisplayMode display,
      const favicon_base::FaviconRawBitmapResult& bitmap_result,
      blink::WebScreenOrientationLockType orientation);

  // Adds a shortcut to the launcher using a SkBitmap.
  // Must be called from a WorkerPool task.
  static void AddShortcutInBackgroundWithSkBitmap(
      const GURL& url,
      const base::string16& title,
      content::Manifest::DisplayMode display,
      const SkBitmap& icon_bitmap,
      blink::WebScreenOrientationLockType orientation);

  // Registers JNI hooks.
  static bool RegisterShortcutHelper(JNIEnv* env);

 private:
  enum ManifestIconStatus {
    MANIFEST_ICON_STATUS_NONE,
    MANIFEST_ICON_STATUS_FETCHING,
    MANIFEST_ICON_STATUS_DONE
  };

  virtual ~ShortcutHelper();

  void Destroy();

  // Runs the algorithm to find the best matching icon in the icons listed in
  // the Manifest.
  // Returns the icon url if a suitable icon is found. An empty URL otherwise.
  GURL FindBestMatchingIcon(
      const std::vector<content::Manifest::Icon>& icons) const;

  // Runs an algorithm only based on icon declared sizes. It will try to find
  // size that is the closest to preferred_icon_size_in_px_ but bigger than
  // preferred_icon_size_in_px_ if possible.
  // Returns the icon url if a suitable icon is found. An empty URL otherwise.
  GURL FindBestMatchingIcon(const std::vector<content::Manifest::Icon>& icons,
                            float density) const;

  // Returns an array containing the items in |icons| without the unsupported
  // image MIME types.
  static std::vector<content::Manifest::Icon> FilterIconsByType(
      const std::vector<content::Manifest::Icon>& icons);

  // Returns whether the preferred_icon_size_in_px_ is in the given |sizes|.
  bool IconSizesContainsPreferredSize(
      const std::vector<gfx::Size>& sizes) const;

  // Returns whether the 'any' (ie. gfx::Size(0,0)) is in the given |sizes|.
  bool IconSizesContainsAny(const std::vector<gfx::Size>& sizes) const;

  JavaObjectWeakGlobalRef java_ref_;

  GURL url_;
  base::string16 title_;
  content::Manifest::DisplayMode display_;
  SkBitmap manifest_icon_;
  base::CancelableTaskTracker cancelable_task_tracker_;
  blink::WebScreenOrientationLockType orientation_;

  bool add_shortcut_requested_;

  ManifestIconStatus manifest_icon_status_;
  const int preferred_icon_size_in_px_;
  static const int kPreferredIconSizeInDp;

  base::WeakPtrFactory<ShortcutHelper> weak_ptr_factory_;

  friend class ShortcutHelperTest;
  DISALLOW_COPY_AND_ASSIGN(ShortcutHelper);
};

#endif  // CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
