// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_BASE_H_
#define COMPONENTS_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_BASE_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "components/renderer_context_menu/context_menu_content_type.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/page_transition_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/window_open_disposition.h"

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace gfx {
class Point;
}

namespace blink {
struct WebMediaPlayerAction;
struct WebPluginAction;
}

class RenderViewContextMenuBase : public ui::SimpleMenuModel::Delegate,
                                  public RenderViewContextMenuProxy {
 public:
  // A delegate interface to communicate with the toolkit used by
  // the embedder.
  class ToolkitDelegate {
   public:
    virtual ~ToolkitDelegate() {}
    // Initialize the toolkit's menu.
    virtual void Init(ui::SimpleMenuModel* menu_model) = 0;

    virtual void Cancel() = 0;

    // Updates the actual menu items controlled by the toolkit.
    virtual void UpdateMenuItem(int command_id,
                                bool enabled,
                                bool hidden,
                                const base::string16& title) = 0;
  };

  static const size_t kMaxSelectionTextLength;

  static void SetContentCustomCommandIdRange(int first, int last);

  // Convert a command ID so that it fits within the range for
  // content context menu.
  static int ConvertToContentCustomCommandId(int id);

  // True if the given id is the one generated for content context menu.
  static bool IsContentCustomCommandId(int id);

  RenderViewContextMenuBase(content::RenderFrameHost* render_frame_host,
                            const content::ContextMenuParams& params);

  virtual ~RenderViewContextMenuBase();

  // Initializes the context menu.
  void Init();

  // Programmatically closes the context menu.
  void Cancel();

  const ui::SimpleMenuModel& menu_model() const { return menu_model_; }
  const content::ContextMenuParams& params() const { return params_; }

  // Returns true if the specified command id is known and valid for
  // this menu. If the command is known |enabled| is set to indicate
  // if the command is enabled.
  bool IsCommandIdKnown(int command_id, bool* enabled) const;

  // SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;
  virtual void MenuWillShow(ui::SimpleMenuModel* source) OVERRIDE;
  virtual void MenuClosed(ui::SimpleMenuModel* source) OVERRIDE;

  // RenderViewContextMenuProxy implementation.
  virtual void AddMenuItem(int command_id,
                           const base::string16& title) OVERRIDE;
  virtual void AddCheckItem(int command_id,
                            const base::string16& title) OVERRIDE;
  virtual void AddSeparator() OVERRIDE;
  virtual void AddSubMenu(int command_id,
                          const base::string16& label,
                          ui::MenuModel* model) OVERRIDE;
  virtual void UpdateMenuItem(int command_id,
                              bool enabled,
                              bool hidden,
                              const base::string16& title) OVERRIDE;
  virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE;

 protected:
  friend class RenderViewContextMenuTest;
  friend class RenderViewContextMenuPrefsTest;

  void set_content_type(ContextMenuContentType* content_type) {
    content_type_.reset(content_type);
  }

  void set_toolkit_delegate(scoped_ptr<ToolkitDelegate> delegate) {
    toolkit_delegate_ = delegate.Pass();
  }

  ToolkitDelegate* toolkit_delegate() {
    return toolkit_delegate_.get();
  }

  // TODO(oshima): Make these methods delegate.

  // Menu Construction.
  virtual void InitMenu();

  // Increments histogram value for used items specified by |id|.
  virtual void RecordUsedItem(int id) = 0;

  // Increments histogram value for visible context menu item specified by |id|.
  virtual void RecordShownItem(int id) = 0;

#if defined(ENABLE_PLUGINS)
  virtual void HandleAuthorizeAllPlugins() = 0;
#endif

  // Returns the accelerator for given |command_id|.
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) = 0;

  // Subclasses should send notification.
  virtual void NotifyMenuShown() = 0;
  virtual void NotifyURLOpened(const GURL& url,
                               content::WebContents* new_contents) = 0;

  // TODO(oshima): Remove this.
  virtual void AppendPlatformEditableItems() {}

  content::RenderFrameHost* GetRenderFrameHost();

  bool IsCustomItemChecked(int id) const;
  bool IsCustomItemEnabled(int id) const;

  // Opens the specified URL string in a new tab.
  void OpenURL(const GURL& url, const GURL& referrer,
               WindowOpenDisposition disposition,
               content::PageTransition transition);

  content::ContextMenuParams params_;
  content::WebContents* source_web_contents_;
  content::BrowserContext* browser_context_;

  ui::SimpleMenuModel menu_model_;

  // Our observers.
  mutable ObserverList<RenderViewContextMenuObserver> observers_;

  // Whether a command has been executed. Used to track whether menu observers
  // should be notified of menu closing without execution.
  bool command_executed_;

  scoped_ptr<ContextMenuContentType> content_type_;

 private:
  bool AppendCustomItems();

  // The RenderFrameHost's IDs.
  int render_process_id_;
  int render_frame_id_;

  scoped_ptr<ToolkitDelegate> toolkit_delegate_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContextMenuBase);
};

#endif  // COMPONENTS_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_BASE_H_
