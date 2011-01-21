// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/options/internet_page_view.h"
#include "chrome/browser/chromeos/options/system_page_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/options/advanced_page_gtk.h"
#include "chrome/browser/ui/gtk/options/content_page_gtk.h"
#include "chrome/browser/ui/gtk/options/general_page_gtk.h"
#include "chrome/browser/ui/options/options_window.h"
#include "chrome/browser/ui/views/accessible_view_helper.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/native/native_view_host.h"
#include "views/controls/tabbed_pane/native_tabbed_pane_gtk.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/widget/root_view.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"
#include "views/window/window_gtk.h"


namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// GtkPreferencePageHost
//
//  Hosts a GTK preference page and takes care of sizing it appropriately.
//
class GtkPreferencePageHost : public views::NativeViewHost {
 public:
  explicit GtkPreferencePageHost(GtkWidget* widget);

 private:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  virtual gfx::Size GetPreferredSize();

  GtkWidget* widget_;

  DISALLOW_COPY_AND_ASSIGN(GtkPreferencePageHost);
};

GtkPreferencePageHost::GtkPreferencePageHost(GtkWidget* widget)
    : widget_(widget) {
  set_background(views::Background::CreateSolidBackground(SK_ColorWHITE));
}

void GtkPreferencePageHost::ViewHierarchyChanged(bool is_add,
                                                 View* parent,
                                                 View* child) {
  NativeViewHost::ViewHierarchyChanged(is_add, parent, child);
  if (is_add && child == this)
    Attach(widget_);
}

gfx::Size GtkPreferencePageHost::GetPreferredSize() {
  // We need to show the widget and its children since otherwise containers like
  // gtk_box don't compute the correct size.
  gtk_widget_show_all(widget_);
  GtkRequisition requisition = { 0, 0 };
  gtk_widget_size_request(GTK_WIDGET(widget_), &requisition);
  GtkRequisition& size(widget_->requisition);
  return gfx::Size(size.width, size.height);
}

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView
//
//  The contents of the Options dialog window.
//
class OptionsWindowView : public views::View,
                          public views::DialogDelegate,
                          public views::TabbedPane::Listener {
 public:
  static const int kDialogPadding;
  static const SkColor kDialogBackground;
  static OptionsWindowView* instance_;

  explicit OptionsWindowView(Profile* profile);
  virtual ~OptionsWindowView();

  // Shows the Tab corresponding to the specified OptionsPage.
  void ShowOptionsPage(OptionsPage page, OptionsGroup highlight_group);

  // views::DialogDelegate implementation:
  virtual int GetDialogButtons() const {
    return 0;
  }
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual bool Cancel();
  virtual views::View* GetContentsView();
  virtual bool ShouldShowClientEdge() const {
    return false;
  }

  // views::TabbedPane::Listener implementation:
  virtual void TabSelectedAt(int index);

  // views::View overrides:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);
 private:
  // Init the assorted Tabbed pages
  void Init();

  // Returns the currently selected OptionsPageView.
  OptionsPageView* GetCurrentOptionsPageView() const;

  // Flag of whether we are fully initialized.
  bool initialized_;

  // The Tab view that contains all of the options pages.
  views::TabbedPane* tabs_;

  // The Profile associated with these options.
  Profile* profile_;

  // Native Gtk options pages.
  GeneralPageGtk general_page_;
  ContentPageGtk content_page_;
  AdvancedPageGtk advanced_page_;

  // The last page the user was on when they opened the Options window.
  IntegerPrefMember last_selected_page_;

  scoped_ptr<AccessibleViewHelper> accessible_view_helper_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(OptionsWindowView);
};

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView, public:

// No dialog padding.
const int OptionsWindowView::kDialogPadding = 0;

// Shaded frame color that matches the rendered frame. It's the result
// of SRC_OVER frame top center image on top of frame color.
const SkColor OptionsWindowView::kDialogBackground =
    SkColorSetRGB(0x4E, 0x7D, 0xD0);

// Live instance of the options view.
OptionsWindowView* OptionsWindowView::instance_ = NULL;

OptionsWindowView::OptionsWindowView(Profile* profile)
      // Always show preferences for the original profile. Most state when off
      // the record comes from the original profile, but we explicitly use
      // the original profile to avoid potential problems.
    : initialized_(false),
      tabs_(NULL),
      profile_(profile->GetOriginalProfile()),
      general_page_(profile_),
      content_page_(profile_),
      advanced_page_(profile_) {
  // We don't need to observe changes in this value.
  last_selected_page_.Init(prefs::kOptionsWindowLastTabIndex,
                           g_browser_process->local_state(), NULL);

  set_background(views::Background::CreateSolidBackground(kDialogBackground));
}

OptionsWindowView::~OptionsWindowView() {
}

void OptionsWindowView::ShowOptionsPage(OptionsPage page,
                                        OptionsGroup highlight_group) {
  // This will show invisible windows and bring visible windows to the front.
  window()->Show();

  // Show all controls in tab after tab's size is allocated. We have
  // to do this after size allocation otherwise WrapLabelAtAllocationHack
  // in ContentPageGtk would break.
  GtkWidget* notebook = static_cast<views::NativeTabbedPaneGtk*>(
      tabs_->native_wrapper())->native_view();
  gtk_widget_show_all(notebook);

  if (page == OPTIONS_PAGE_DEFAULT) {
    // Remember the last visited page from local state.
    page = static_cast<OptionsPage>(last_selected_page_.GetValue());
    if (page == OPTIONS_PAGE_DEFAULT)
      page = OPTIONS_PAGE_GENERAL;
  }
  // If the page number is out of bounds, reset to the first tab.
  if (page < 0 || page >= tabs_->GetTabCount())
    page = OPTIONS_PAGE_GENERAL;

  tabs_->SelectTabAt(static_cast<int>(page));

  // TODO(xiyuan): set highlight_group
}

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView, views::DialogDelegate implementation:

std::wstring OptionsWindowView::GetWindowTitle() const {
  return UTF16ToWide(
      l10n_util::GetStringFUTF16(IDS_OPTIONS_DIALOG_TITLE,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
}

void OptionsWindowView::WindowClosing() {
  // Clear the static instance so that the next time ShowOptionsWindow() is
  // called a new window is opened.
  instance_ = NULL;
}

bool OptionsWindowView::Cancel() {
  OptionsPageView* selected_option_page = GetCurrentOptionsPageView();
  if (selected_option_page) {
    return selected_option_page->CanClose();
  } else {
    // Gtk option pages does not support CanClose.
    return true;
  }
}

views::View* OptionsWindowView::GetContentsView() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView, views::TabbedPane::Listener implementation:

void OptionsWindowView::TabSelectedAt(int index) {
  if (!initialized_)
    return;

  DCHECK(index > OPTIONS_PAGE_DEFAULT && index < OPTIONS_PAGE_COUNT);
  last_selected_page_.SetValue(index);
}

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView, views::View overrides:

void OptionsWindowView::Layout() {
  tabs_->SetBounds(kDialogPadding, kDialogPadding,
                   width() - (2 * kDialogPadding),
                   height() - (2 * kDialogPadding));
  if (!accessible_view_helper_.get()) {
    accessible_view_helper_.reset(new AccessibleViewHelper(this, profile_));
  }
}

gfx::Size OptionsWindowView::GetPreferredSize() {
  gfx::Size size(tabs_->GetPreferredSize());
  size.Enlarge(2 * kDialogPadding, 2 * kDialogPadding);
  return size;
}

void OptionsWindowView::ViewHierarchyChanged(bool is_add,
                                             views::View* parent,
                                             views::View* child) {
  // Can't init before we're inserted into a Container, because we require a
  // HWND to parent native child controls to.
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowView, private:

void OptionsWindowView::Init() {
  tabs_ = new views::TabbedPane;
  tabs_->SetListener(this);
  AddChildView(tabs_);

  GtkWidget* notebook = static_cast<views::NativeTabbedPaneGtk*>(
      tabs_->native_wrapper())->native_view();

  // Set vertical padding of tab header.
  gtk_notebook_set_tab_vborder(GTK_NOTEBOOK(notebook), 5);

  // Set a name for notebook. Note that the name is also used in theme engine
  // to apply specific drawings for this widget.
  gtk_widget_set_name(GTK_WIDGET(notebook), "chromeos-options-tab");

  // Setup tab pages.
  int tab_index = 0;

  SystemPageView* system_page = new SystemPageView(profile_);
  system_page->set_background(views::Background::CreateSolidBackground(
      SK_ColorWHITE));
  tabs_->AddTabAtIndex(tab_index++,
                       UTF16ToWide(l10n_util::GetStringUTF16(
                           IDS_OPTIONS_SYSTEM_TAB_LABEL)),
                       system_page, false);

  InternetPageView* internet_page = new InternetPageView(profile_);
  internet_page->set_background(views::Background::CreateSolidBackground(
      SK_ColorWHITE));
  tabs_->AddTabAtIndex(tab_index++,
                       UTF16ToWide(l10n_util::GetStringUTF16(
                           IDS_OPTIONS_INTERNET_TAB_LABEL)),
                       internet_page, false);

  tabs_->AddTabAtIndex(tab_index++,
                       UTF16ToWide(l10n_util::GetStringUTF16(
                           IDS_OPTIONS_GENERAL_TAB_LABEL)),
                       new GtkPreferencePageHost(
                           general_page_.get_page_widget()),
                       false);

  tabs_->AddTabAtIndex(tab_index++,
                       UTF16ToWide(l10n_util::GetStringUTF16(
                           IDS_OPTIONS_CONTENT_TAB_LABEL)),
                       new GtkPreferencePageHost(
                           content_page_.get_page_widget()),
                       false);

  tabs_->AddTabAtIndex(tab_index++,
                       UTF16ToWide(l10n_util::GetStringUTF16(
                           IDS_OPTIONS_ADVANCED_TAB_LABEL)),
                       new GtkPreferencePageHost(
                           advanced_page_.get_page_widget()),
                       false);

  DCHECK(tabs_->GetTabCount() == OPTIONS_PAGE_COUNT);

  initialized_ = true;
}

OptionsPageView* OptionsWindowView::GetCurrentOptionsPageView() const {
  int selected_option_page = tabs_->GetSelectedTabIndex();
  if (selected_option_page < OPTIONS_PAGE_GENERAL) {
    return static_cast<OptionsPageView*>(tabs_->GetSelectedTab());
  } else {
    return NULL;
  }
}

void CloseOptionsWindow() {
  if (OptionsWindowView::instance_)
    OptionsWindowView::instance_->window()->Close();
}

gfx::NativeWindow GetOptionsViewParent() {
  if (Browser* b = BrowserList::GetLastActive()) {
    if (b->type() != Browser::TYPE_NORMAL) {
      b = BrowserList::FindBrowserWithType(b->profile(),
                                           Browser::TYPE_NORMAL,
                                           true);
    }

    if (b)
      return b->window()->GetNativeHandle();
  }

  return NULL;
}

};  // namespace chromeos

///////////////////////////////////////////////////////////////////////////////
// Factory/finder method:

void ShowOptionsWindow(OptionsPage page,
                       OptionsGroup highlight_group,
                       Profile* profile) {
  DCHECK(profile);

  using chromeos::OptionsWindowView;

  // If there's already an existing options window, close it and create
  // a new one for the current active browser.
  chromeos::CloseOptionsWindow();

  OptionsWindowView::instance_ = new OptionsWindowView(profile);
  browser::CreateViewsWindow(chromeos::GetOptionsViewParent(),
                             gfx::Rect(),
                             OptionsWindowView::instance_);

  OptionsWindowView::instance_->ShowOptionsPage(page, highlight_group);
}
