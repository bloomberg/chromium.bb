// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BLIMP_ENGINE_BROWSER_BLIMP_WINDOW_H_
#define BLIMP_ENGINE_BROWSER_BLIMP_WINDOW_H_

#include <vector>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/size.h"

namespace content {
class BrowserContext;
class SiteInstance;
class WebContents;
}

class GURL;

namespace blimp {
namespace engine {

// Owns and controls a WebContents instance corresponding to a window on
// Blimp client.
class BlimpWindow : public content::WebContentsDelegate {
 public:
  // This also unregisters itself with a singleton registry.
  ~BlimpWindow() override;

  // Creates a new blimp window with |initial_size| and navigates to the |url|.
  // Caller retains ownership of |browser_context| and |site_instance| and
  // ensures |browser_context| and |site_instance| outlives BlimpWindow.
  static void Create(content::BrowserContext* browser_context,
                     const GURL& url,
                     content::SiteInstance* site_instance,
                     const gfx::Size& initial_size);

  // Navigates to |url|.
  void LoadURL(const GURL& url);

  // content::WebContentsDelegate implementation.
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void AddNewContents(content::WebContents* source,
                      content::WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  void RequestToLockMouse(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override;
  void CloseContents(content::WebContents* source) override;
  void ActivateContents(content::WebContents* contents) override;
  void DeactivateContents(content::WebContents* contents) override;

 private:
  // The newly created instance registers itself with a singleton registry.
  explicit BlimpWindow(scoped_ptr<content::WebContents> web_contents);

  // Helper to create a new BlimpWindow given |web_contents|.
  // The newly window is owned by a singleton registry.
  static BlimpWindow* DoCreate(scoped_ptr<content::WebContents> web_contents,
                               const gfx::Size& initial_size);

  scoped_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(BlimpWindow);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_BROWSER_BLIMP_WINDOW_H_
