// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SHELL_BROWSER_ANDROID_CAST_WINDOW_ANDROID_H_
#define CHROMECAST_SHELL_BROWSER_ANDROID_CAST_WINDOW_ANDROID_H_

#include <jni.h>
#include <vector>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

class GURL;

namespace content {
class BrowserContext;
class SiteInstance;
class WebContents;
}  // namespace content

namespace chromecast {
namespace shell {

class CastWindowAndroid : public content::WebContentsDelegate,
                          public content::WebContentsObserver {
 public:
  // Creates a new window and immediately loads the given URL.
  static CastWindowAndroid* CreateNewWindow(
      content::BrowserContext* browser_context,
      const GURL& url);

  virtual ~CastWindowAndroid();

  void LoadURL(const GURL& url);
  // Calls RVH::ClosePage() and waits for acknowledgement before closing/
  // deleting the window.
  void Close();
  // Destroys this window immediately.
  void Destroy();

  // Registers the JNI methods for CastWindowAndroid.
  static bool RegisterJni(JNIEnv* env);

  // content::WebContentsDelegate implementation:
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void CloseContents(content::WebContents* source) OVERRIDE;
  virtual bool CanOverscrollContent() const OVERRIDE;
  virtual bool AddMessageToConsole(content::WebContents* source,
                                   int32 level,
                                   const base::string16& message,
                                   int32 line_no,
                                   const base::string16& source_id) OVERRIDE;
  virtual void ActivateContents(content::WebContents* contents) OVERRIDE;
  virtual void DeactivateContents(content::WebContents* contents) OVERRIDE;

  // content::WebContentsObserver implementation:
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;

 private:
  explicit CastWindowAndroid(content::WebContents* web_contents);

  // Helper to create a new CastWindowAndroid given a newly created WebContents.
  static CastWindowAndroid* CreateCastWindowAndroid(
      content::WebContents* web_contents,
      const gfx::Size& initial_size);

  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  scoped_ptr<content::WebContents> web_contents_;

  base::WeakPtrFactory<CastWindowAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastWindowAndroid);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_SHELL_BROWSER_ANDROID_CAST_WINDOW_ANDROID_H_
