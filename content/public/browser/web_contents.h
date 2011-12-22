// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
#pragma once

#include "base/basictypes.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "content/common/content_export.h"

class NavigationController;
class RenderViewHost;
class RenderViewHostManager;
class RenderWidgetHostView;
class SiteInstance;
// TODO(jam): of course we will have to rename TabContentsView etc to use
// WebContents.
class TabContentsView;
class WebUI;

namespace base {
class PropertyBag;
class TimeTicks;
}

namespace net {
struct LoadStateWithParam;
}

namespace content {

class BrowserContext;
class RenderProcessHost;
class WebContentsDelegate;

// Describes what goes in the main content area of a tab.
class WebContents {
 public:
  // Intrinsic tab state -------------------------------------------------------

  // Returns the property bag for this tab contents, where callers can add
  // extra data they may wish to associate with the tab. Returns a pointer
  // rather than a reference since the PropertyAccessors expect this.
  virtual const base::PropertyBag* GetPropertyBag() const = 0;
  virtual base::PropertyBag* GetPropertyBag() = 0;

  // Gets/Sets the delegate.
  virtual WebContentsDelegate* GetDelegate() = 0;
  virtual void SetDelegate(WebContentsDelegate* delegate) = 0;

  // Gets the controller for this tab contents.
  virtual NavigationController& GetController() = 0;
  virtual const NavigationController& GetController() const = 0;

  // Returns the user browser context associated with this WebContents (via the
  // NavigationController).
  virtual content::BrowserContext* GetBrowserContext() const = 0;

  // Allows overriding the type of this tab.
  virtual void SetViewType(content::ViewType type) = 0;

  // Return the currently active RenderProcessHost and RenderViewHost. Each of
  // these may change over time.
  virtual RenderProcessHost* GetRenderProcessHost() const = 0;

  // Gets the current RenderViewHost for this tab.
  virtual RenderViewHost* GetRenderViewHost() const = 0;

  // Returns the currently active RenderWidgetHostView. This may change over
  // time and can be NULL (during setup and teardown).
  virtual RenderWidgetHostView* GetRenderWidgetHostView() const = 0;

  // The TabContentsView will never change and is guaranteed non-NULL.
  virtual TabContentsView* GetView() const = 0;

  // Returns the committed WebUI if one exists, otherwise the pending one.
  // Callers who want to use the pending WebUI for the pending navigation entry
  // should use GetWebUIForCurrentState instead.
  virtual WebUI* GetWebUI() const = 0;
  virtual WebUI* GetCommittedWebUI() const = 0;

  // Tab navigation state ------------------------------------------------------

  // Returns the current navigation properties, which if a navigation is
  // pending may be provisional (e.g., the navigation could result in a
  // download, in which case the URL would revert to what it was previously).
  virtual const string16& GetTitle() const = 0;

  // The max page ID for any page that the current SiteInstance has loaded in
  // this TabContents.  Page IDs are specific to a given SiteInstance and
  // TabContents, corresponding to a specific RenderView in the renderer.
  // Page IDs increase with each new page that is loaded by a tab.
  virtual int32 GetMaxPageID() = 0;

  // The max page ID for any page that the given SiteInstance has loaded in
  // this TabContents.
  virtual int32 GetMaxPageIDForSiteInstance(SiteInstance* site_instance) = 0;

  // Returns the SiteInstance associated with the current page.
  virtual SiteInstance* GetSiteInstance() const = 0;

  // Returns the SiteInstance for the pending navigation, if any.  Otherwise
  // returns the current SiteInstance.
  virtual SiteInstance* GetPendingSiteInstance() const = 0;

  // Return whether this tab contents is loading a resource.
  virtual bool IsLoading() const = 0;

  // Returns whether this tab contents is waiting for a first-response for the
  // main resource of the page.
  virtual bool IsWaitingForResponse() const = 0;

  // Return the current load state and the URL associated with it.
  virtual const net::LoadStateWithParam& GetLoadState() const = 0;
  virtual const string16& GetLoadStateHost() const = 0;

  // Return the upload progress.
  virtual uint64 GetUploadSize() const = 0;
  virtual uint64 GetUploadPosition() const = 0;

  // Return the character encoding of the page.
  virtual const std::string& GetEncoding() const = 0;

  // True if this is a secure page which displayed insecure content.
  virtual bool DisplayedInsecureContent() const = 0;

  // Internal state ------------------------------------------------------------

  // This flag indicates whether the tab contents is currently being
  // screenshotted by the DraggedTabController.
  virtual void SetCapturingContents(bool cap)  = 0;

  // Indicates whether this tab should be considered crashed. The setter will
  // also notify the delegate when the flag is changed.
  virtual bool IsCrashed() const  = 0;
  virtual void SetIsCrashed(base::TerminationStatus status, int error_code) = 0;

  virtual base::TerminationStatus GetCrashedStatus() const  = 0;

  // Whether the tab is in the process of being destroyed.
  // Added as a tentative work-around for focus related bug #4633.  This allows
  // us not to store focus when a tab is being closed.
  virtual bool IsBeingDestroyed() const  = 0;

  // Convenience method for notifying the delegate of a navigation state
  // change. See WebContentsDelegate.
  virtual void NotifyNavigationStateChanged(unsigned changed_flags) = 0;

  // Invoked when the tab contents becomes selected. If you override, be sure
  // and invoke super's implementation.
  virtual void DidBecomeSelected() = 0;
  virtual base::TimeTicks GetLastSelectedTime() const = 0;

  // Invoked when the tab contents becomes hidden.
  // NOTE: If you override this, call the superclass version too!
  virtual void WasHidden() = 0;

  // TODO(brettw) document these.
  virtual void ShowContents() = 0;
  virtual void HideContents() = 0;

  // Returns true if the before unload and unload listeners need to be
  // fired. The value of this changes over time. For example, if true and the
  // before unload listener is executed and allows the user to exit, then this
  // returns false.
  virtual bool NeedToFireBeforeUnload() = 0;

  // Expose the render manager for testing.
  virtual RenderViewHostManager* GetRenderManagerForTesting() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
