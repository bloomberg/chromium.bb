// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_MANIFEST_UPGRADE_DETECTOR_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_MANIFEST_UPGRADE_DETECTOR_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
struct Manifest;
class WebContents;
}

class GURL;
struct ShortcutInfo;

// ManifestUpgradeDetector is the C++ counterpart of
// org.chromium.chrome.browser's ManifestUpgradeDetector in Java. It is created
// via a JNI (Initialize) call and MUST BE DESTROYED via Destroy().
class ManifestUpgradeDetector : public content::WebContentsObserver {
 public:
  ManifestUpgradeDetector(JNIEnv* env,
                          jobject obj,
                          content::WebContents* web_contents,
                          const GURL& scope,
                          const GURL& web_manifest_url);

  // Replaces the WebContents that is being observed.
  void ReplaceWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents);

  // Called by the Java counterpart to destroy its native half.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Called by the Java counterpart to start checking web manifest changes.
  void Start(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Registers JNI hooks.
  static bool Register(JNIEnv* env);

 private:
  FRIEND_TEST_ALL_PREFIXES(ManifestUpgradeDetectorTest,
              OnDidGetManifestReturnsFalseWhenTheFetchedManifestUrlIsEmpty);
  ~ManifestUpgradeDetector() override;

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // Called when the Manifest has been parsed, or if no Manifest was found.
  void OnDidGetManifest(const GURL& manifest_url,
                        const content::Manifest& manifest);

  void OnDataAvailable(const ShortcutInfo& info);

  // Points to the Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // A flag to indicate if the detection pipeline was started.
  bool started_;

  // The detector will only fetch the URL within the scope of the WebAPK.
  const GURL scope_;

  // The WebAPK's Web Manifest URL that the detector is looking for.
  const GURL web_manifest_url_;

  base::WeakPtrFactory<ManifestUpgradeDetector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManifestUpgradeDetector);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_MANIFEST_UPGRADE_DETECTOR_H_
