// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the public interface for the blocked content (including popup)
// notifications. This interface should only be used by the
// BlockedContentTabHelper. Users and subclasses of WebContents
// should use the appropriate methods on BlockedContentTabHelper to access
// information about blocked content.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_CONTENT_CONTAINER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_CONTENT_CONTAINER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper_delegate.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {
class WebContents;
}

// Takes ownership of WebContentses that are unrequested popup windows.
class BlockedContentContainer : public BlockedContentTabHelperDelegate,
                                public content::WebContentsDelegate {
 public:
  // Creates a container for a certain WebContents:
  explicit BlockedContentContainer(content::WebContents* owner);
  virtual ~BlockedContentContainer();

  // Adds a WebContents to this container. |bounds| are the window bounds
  // requested for the WebContents.
  void AddWebContents(content::WebContents* web_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& bounds,
                      bool user_gesture);

  // Shows the blocked WebContents |web_contents|.
  void LaunchForContents(content::WebContents* web_contents);

  // Returns the number of blocked contents.
  size_t GetBlockedContentsCount() const;

  // Returns the contained WebContents pointers.  |blocked_contents| must
  // be non-NULL.
  void GetBlockedContents(
      std::vector<content::WebContents*>* blocked_contents) const;

  // Removes all blocked contents.
  void Clear();

  // Overridden from BlockedContentTabHelperDelegate:
  virtual content::WebContents* GetConstrainingWebContents(
      content::WebContents* source) OVERRIDE;

  // Overridden from content::WebContentsDelegate:

  // Forwards OpenURLFromTab to our |owner_|.
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;

  // Forwards AddNewContents to our |owner_|.
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_position,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;

  // Removes |source| from our internal list of blocked contents.
  virtual void CloseContents(content::WebContents* source) OVERRIDE;

  // Changes the opening rectangle associated with |source|.
  virtual void MoveContents(content::WebContents* source,
                            const gfx::Rect& new_bounds) OVERRIDE;

  virtual bool IsPopupOrPanel(
      const content::WebContents* source) const OVERRIDE;

  // Always returns true.
  virtual bool ShouldSuppressDialogs() OVERRIDE;

  // Maximum number of blocked contents we allow. No page should really need
  // this many anyway. If reached it typically means there is a compromised
  // renderer.
  static const size_t kImpossibleNumberOfPopups;

 private:
  struct BlockedContent;

  typedef std::vector<BlockedContent> BlockedContents;

  // The WebContents that owns and constrains this BlockedContentContainer.
  content::WebContents* owner_;

  // Information about all blocked contents.
  BlockedContents blocked_contents_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BlockedContentContainer);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_CONTENT_CONTAINER_H_
