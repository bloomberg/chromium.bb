// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HUNG_RENDERER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_HUNG_RENDERER_VIEW_H_

#include "base/macros.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_unresponsive_state.h"
#include "ui/base/models/table_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/table/table_grouper.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace views {
class Label;
}

// Provides functionality to display information about a hung renderer.
class HungPagesTableModel : public ui::TableModel, public views::TableGrouper {
 public:
  // The Delegate is notified any time a WebContents the model is listening to
  // is destroyed.
  class Delegate {
   public:
    virtual void TabDestroyed() = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit HungPagesTableModel(Delegate* delegate);
  ~HungPagesTableModel() override;

  void InitForWebContents(content::WebContents* hung_contents);

  // Returns the first RenderProcessHost, or NULL if there aren't any
  // WebContents.
  content::RenderProcessHost* GetRenderProcessHost();

  // Returns the first RenderViewHost, or NULL if there aren't any WebContents.
  content::RenderViewHost* GetRenderViewHost();

  // Overridden from ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int row, int column_id) override;
  gfx::ImageSkia GetIcon(int row) override;
  void SetObserver(ui::TableModelObserver* observer) override;

  // Overridden from views::TableGrouper:
  void GetGroupRange(int model_index, views::GroupRange* range) override;

 private:
  // Used to track a single WebContents. If the WebContents is destroyed
  // TabDestroyed() is invoked on the model.
  class WebContentsObserverImpl : public content::WebContentsObserver {
   public:
    WebContentsObserverImpl(HungPagesTableModel* model,
                            content::WebContents* tab);

    favicon::FaviconDriver* favicon_driver() {
      return favicon::ContentFaviconDriver::FromWebContents(web_contents());
    }

    // WebContentsObserver overrides:
    void RenderProcessGone(base::TerminationStatus status) override;
    void WebContentsDestroyed() override;

   private:
    HungPagesTableModel* model_;

    DISALLOW_COPY_AND_ASSIGN(WebContentsObserverImpl);
  };

  // Invoked when a WebContents is destroyed. Cleans up |tab_observers_| and
  // notifies the observer and delegate.
  void TabDestroyed(WebContentsObserverImpl* tab);

  std::vector<std::unique_ptr<WebContentsObserverImpl>> tab_observers_;

  ui::TableModelObserver* observer_;
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(HungPagesTableModel);
};

// This class displays a dialog which contains information about a hung
// renderer process.
class HungRendererDialogView : public views::DialogDelegateView,
                               public HungPagesTableModel::Delegate {
 public:
  // Factory function for creating an instance of the HungRendererDialogView
  // class. At any given point only one instance can be active.
  static HungRendererDialogView* Create(gfx::NativeWindow context);

  // Returns a pointer to the singleton instance if any.
  static HungRendererDialogView* GetInstance();

  // Shows or hides the hung renderer dialog for the given WebContents.
  static void Show(
      content::WebContents* contents,
      const content::WebContentsUnresponsiveState& unresponsive_state);
  static void Hide(content::WebContents* contents);

  // Returns true if the frame is in the foreground.
  static bool IsFrameActive(content::WebContents* contents);

  virtual void ShowForWebContents(
      content::WebContents* contents,
      const content::WebContentsUnresponsiveState& unresponsive_state);
  virtual void EndForWebContents(content::WebContents* contents);

  // views::DialogDelegateView overrides:
  base::string16 GetWindowTitle() const override;
  void WindowClosing() override;
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;
  bool ShouldUseCustomFrame() const override;

  // HungPagesTableModel::Delegate overrides:
  void TabDestroyed() override;

 protected:
  HungRendererDialogView();
  ~HungRendererDialogView() override;

  // views::View overrides:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  static HungRendererDialogView* g_instance_;

 private:
  // Initialize the controls in this dialog.
  void Init();

  static void InitClass();

  // An amusing icon image.
  static gfx::ImageSkia* frozen_icon_;

  // The label describing the list.
  views::Label* info_label_;

  // Controls within the dialog box.
  views::TableView* hung_pages_table_;

  // The model that provides the contents of the table that shows a list of
  // pages affected by the hang.
  std::unique_ptr<HungPagesTableModel> hung_pages_table_model_;

  // Whether or not we've created controls for ourself.
  bool initialized_;

  // A copy of the unresponsive state which ShowForWebContents was
  // called with.
  content::WebContentsUnresponsiveState unresponsive_state_;

  DISALLOW_COPY_AND_ASSIGN(HungRendererDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HUNG_RENDERER_VIEW_H_
