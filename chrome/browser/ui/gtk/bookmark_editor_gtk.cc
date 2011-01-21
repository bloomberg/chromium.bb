// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/bookmark_editor_gtk.h"

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/bookmark_tree_model.h"
#include "chrome/browser/ui/gtk/bookmark_utils_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "gfx/gtk_util.h"
#include "gfx/point.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"

#if defined(TOOLKIT_VIEWS)
#include "views/controls/menu/menu_2.h"
#else
#include "chrome/browser/ui/gtk/menu_gtk.h"
#endif

namespace {

// Background color of text field when URL is invalid.
const GdkColor kErrorColor = GDK_COLOR_RGB(0xFF, 0xBC, 0xBC);

// Preferred initial dimensions, in pixels, of the folder tree.
static const int kTreeWidth = 300;
static const int kTreeHeight = 150;

}  // namespace

class BookmarkEditorGtk::ContextMenuController
    : public ui::SimpleMenuModel::Delegate {
 public:
  explicit ContextMenuController(BookmarkEditorGtk* editor)
      : editor_(editor),
        running_menu_for_root_(false) {
    menu_model_.reset(new ui::SimpleMenuModel(this));
    menu_model_->AddItemWithStringId(COMMAND_EDIT, IDS_EDIT);
    menu_model_->AddItemWithStringId(
        COMMAND_NEW_FOLDER,
        IDS_BOOMARK_EDITOR_NEW_FOLDER_MENU_ITEM);
#if defined(TOOLKIT_VIEWS)
    menu_.reset(new views::Menu2(menu_model_.get()));
#else
    menu_.reset(new MenuGtk(NULL, menu_model_.get()));
#endif
  }
  virtual ~ContextMenuController() {}

  void RunMenu(const gfx::Point& point) {
    const BookmarkNode* selected_node = GetSelectedNode();
    if (selected_node)
      running_menu_for_root_ = selected_node->GetParent()->IsRoot();
#if defined(TOOLKIT_VIEWS)
    menu_->RunContextMenuAt(point);
#else
    menu_->PopupAsContextAt(gtk_get_current_event_time(), point);
#endif
  }

  void Cancel() {
    editor_ = NULL;
#if defined(TOOLKIT_VIEWS)
    menu_->CancelMenu();
#else
    menu_->Cancel();
#endif
  }

 private:
  enum ContextMenuCommand {
    COMMAND_EDIT,
    COMMAND_NEW_FOLDER
  };

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdEnabled(int command_id) const {
    return !(command_id == COMMAND_EDIT && running_menu_for_root_) &&
        (editor_ != NULL);
  }

  virtual bool IsCommandIdChecked(int command_id) const {
    return false;
  }

  virtual bool GetAcceleratorForCommandId(int command_id,
                                          ui::Accelerator* accelerator) {
    return false;
  }

  virtual void ExecuteCommand(int command_id) {
    if (!editor_)
      return;

    switch (command_id) {
      case COMMAND_EDIT: {
        GtkTreeIter iter;
        if (!gtk_tree_selection_get_selected(editor_->tree_selection_,
                                             NULL,
                                             &iter)) {
          return;
        }

        GtkTreePath* path = gtk_tree_model_get_path(
            GTK_TREE_MODEL(editor_->tree_store_), &iter);
        gtk_tree_view_expand_to_path(GTK_TREE_VIEW(editor_->tree_view_), path);

        // Make the folder name editable.
        gtk_tree_view_set_cursor(GTK_TREE_VIEW(editor_->tree_view_), path,
            gtk_tree_view_get_column(GTK_TREE_VIEW(editor_->tree_view_), 0),
            TRUE);

        gtk_tree_path_free(path);
        break;
      }
      case COMMAND_NEW_FOLDER:
        editor_->NewFolder();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  int64 GetRowIdAt(GtkTreeModel* model, GtkTreeIter* iter) {
    GValue value = { 0, };
    gtk_tree_model_get_value(model, iter, bookmark_utils::ITEM_ID, &value);
    int64 id = g_value_get_int64(&value);
    g_value_unset(&value);
    return id;
  }

  const BookmarkNode* GetNodeAt(GtkTreeModel* model, GtkTreeIter* iter) {
    int64 id = GetRowIdAt(model, iter);
    return (id > 0) ? editor_->bb_model_->GetNodeByID(id) : NULL;
  }

  const BookmarkNode* GetSelectedNode() {
    GtkTreeModel* model;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(editor_->tree_selection_,
                                         &model,
                                         &iter)) {
      return NULL;
    }

    return GetNodeAt(model, &iter);
  }

  // The model and view for the right click context menu.
  scoped_ptr<ui::SimpleMenuModel> menu_model_;
#if defined(TOOLKIT_VIEWS)
  scoped_ptr<views::Menu2> menu_;
#else
  scoped_ptr<MenuGtk> menu_;
#endif

  // The context menu was brought up for. Set to NULL when the menu is canceled.
  BookmarkEditorGtk* editor_;

  // If true, we're running the menu for the bookmark bar or other bookmarks
  // nodes.
  bool running_menu_for_root_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuController);
};

// static
void BookmarkEditor::Show(gfx::NativeWindow parent_hwnd,
                          Profile* profile,
                          const BookmarkNode* parent,
                          const EditDetails& details,
                          Configuration configuration) {
  DCHECK(profile);
  BookmarkEditorGtk* editor =
      new BookmarkEditorGtk(parent_hwnd, profile, parent, details,
                            configuration);
  editor->Show();
}

BookmarkEditorGtk::BookmarkEditorGtk(
    GtkWindow* window,
    Profile* profile,
    const BookmarkNode* parent,
    const EditDetails& details,
    BookmarkEditor::Configuration configuration)
    : profile_(profile),
      dialog_(NULL),
      parent_(parent),
      details_(details),
      running_menu_for_root_(false),
      show_tree_(configuration == SHOW_TREE) {
  DCHECK(profile);
  Init(window);
}

BookmarkEditorGtk::~BookmarkEditorGtk() {
  // The tree model is deleted before the view. Reset the model otherwise the
  // tree will reference a deleted model.

  bb_model_->RemoveObserver(this);
}

void BookmarkEditorGtk::Init(GtkWindow* parent_window) {
  bb_model_ = profile_->GetBookmarkModel();
  DCHECK(bb_model_);
  bb_model_->AddObserver(this);

  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_TITLE).c_str(),
      parent_window,
      GTK_DIALOG_MODAL,
      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
      NULL);
  gtk_dialog_set_has_separator(GTK_DIALOG(dialog_), FALSE);

  if (show_tree_) {
    GtkWidget* action_area = GTK_DIALOG(dialog_)->action_area;
    new_folder_button_ = gtk_button_new_with_label(
        l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_NEW_FOLDER_BUTTON).c_str());
    g_signal_connect(new_folder_button_, "clicked",
                     G_CALLBACK(OnNewFolderClickedThunk), this);
    gtk_container_add(GTK_CONTAINER(action_area), new_folder_button_);
    gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(action_area),
                                       new_folder_button_, TRUE);
  }

  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_ACCEPT);

  // The GTK dialog content area layout (overview)
  //
  // +- GtkVBox |vbox| ----------------------------------------------+
  // |+- GtkTable |table| ------------------------------------------+|
  // ||+- GtkLabel ------+ +- GtkEntry |name_entry_| --------------+||
  // |||                 | |                                       |||
  // ||+-----------------+ +---------------------------------------+||
  // ||+- GtkLabel ------+ +- GtkEntry |url_entry_| ---------------+|| *
  // |||                 | |                                       |||
  // ||+-----------------+ +---------------------------------------+||
  // |+-------------------------------------------------------------+|
  // |+- GtkScrollWindow |scroll_window| ---------------------------+|
  // ||+- GtkTreeView |tree_view_| --------------------------------+||
  // |||+- GtkTreeViewColumn |name_column| -----------------------+|||
  // ||||                                                         ||||
  // ||||                                                         ||||
  // ||||                                                         ||||
  // ||||                                                         ||||
  // |||+---------------------------------------------------------+|||
  // ||+-----------------------------------------------------------+||
  // |+-------------------------------------------------------------+|
  // +---------------------------------------------------------------+
  //
  // * The url and corresponding label are not shown if creating a new folder.
  GtkWidget* content_area = GTK_DIALOG(dialog_)->vbox;
  gtk_box_set_spacing(GTK_BOX(content_area), gtk_util::kContentAreaSpacing);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 12);

  name_entry_ = gtk_entry_new();
  std::string title;
  if (details_.type == EditDetails::EXISTING_NODE) {
    title = UTF16ToUTF8(details_.existing_node->GetTitle());
  } else if (details_.type == EditDetails::NEW_FOLDER) {
    title = l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME);
  }
  gtk_entry_set_text(GTK_ENTRY(name_entry_), title.c_str());
  g_signal_connect(name_entry_, "changed",
                   G_CALLBACK(OnEntryChangedThunk), this);
  gtk_entry_set_activates_default(GTK_ENTRY(name_entry_), TRUE);

  GtkWidget* table;
  if (details_.type != EditDetails::NEW_FOLDER) {
    url_entry_ = gtk_entry_new();
    std::string url_spec;
    if (details_.type == EditDetails::EXISTING_NODE)
      url_spec = details_.existing_node->GetURL().spec();
    gtk_entry_set_text(GTK_ENTRY(url_entry_), url_spec.c_str());
    g_signal_connect(url_entry_, "changed",
                     G_CALLBACK(OnEntryChangedThunk), this);
    gtk_entry_set_activates_default(GTK_ENTRY(url_entry_), TRUE);
    table = gtk_util::CreateLabeledControlsGroup(NULL,
        l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_NAME_LABEL).c_str(),
        name_entry_,
        l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_URL_LABEL).c_str(),
        url_entry_,
        NULL);

  } else {
    url_entry_ = NULL;
    table = gtk_util::CreateLabeledControlsGroup(NULL,
        l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_NAME_LABEL).c_str(),
        name_entry_,
        NULL);
  }

  gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

  if (show_tree_) {
    GtkTreeIter selected_iter;
    int64 selected_id = 0;
    if (details_.type == EditDetails::EXISTING_NODE)
      selected_id = details_.existing_node->GetParent()->id();
    else if (parent_)
      selected_id = parent_->id();
    tree_store_ = bookmark_utils::MakeFolderTreeStore();
    bookmark_utils::AddToTreeStore(bb_model_, selected_id, tree_store_,
                                   &selected_iter);
    tree_view_ = bookmark_utils::MakeTreeViewForStore(tree_store_);
    gtk_widget_set_size_request(tree_view_, kTreeWidth, kTreeHeight);
    tree_selection_ = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view_));
    g_signal_connect(tree_view_, "button-press-event",
                     G_CALLBACK(OnTreeViewButtonPressEventThunk), this);

    GtkTreePath* path = NULL;
    if (selected_id) {
      path = gtk_tree_model_get_path(GTK_TREE_MODEL(tree_store_),
                                     &selected_iter);
    } else {
      // We don't have a selected parent (Probably because we're making a new
      // bookmark). Select the first item in the list.
      path = gtk_tree_path_new_from_string("0");
    }

    gtk_tree_view_expand_to_path(GTK_TREE_VIEW(tree_view_), path);
    gtk_tree_selection_select_path(tree_selection_, path);
    gtk_tree_path_free(path);

    GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                    GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                        GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(scroll_window), tree_view_);

    gtk_box_pack_start(GTK_BOX(vbox), scroll_window, TRUE, TRUE, 0);

    g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view_)),
                     "changed", G_CALLBACK(OnSelectionChangedThunk), this);
  }

  gtk_box_pack_start(GTK_BOX(content_area), vbox, TRUE, TRUE, 0);

  g_signal_connect(dialog_, "response",
                   G_CALLBACK(OnResponseThunk), this);
  g_signal_connect(dialog_, "delete-event",
                   G_CALLBACK(OnWindowDeleteEventThunk), this);
  g_signal_connect(dialog_, "destroy",
                   G_CALLBACK(OnWindowDestroyThunk), this);
}

void BookmarkEditorGtk::Show() {
  // Manually call our OnEntryChanged handler to set the initial state.
  OnEntryChanged(NULL);

  gtk_util::ShowDialog(dialog_);
}

void BookmarkEditorGtk::Close() {
  // Under the model that we've inherited from Windows, dialogs can receive
  // more than one Close() call inside the current message loop event.
  if (dialog_) {
    gtk_widget_destroy(dialog_);
    dialog_ = NULL;
  }
}

void BookmarkEditorGtk::BookmarkNodeMoved(BookmarkModel* model,
                                          const BookmarkNode* old_parent,
                                          int old_index,
                                          const BookmarkNode* new_parent,
                                          int new_index) {
  Reset();
}

void BookmarkEditorGtk::BookmarkNodeAdded(BookmarkModel* model,
                                          const BookmarkNode* parent,
                                          int index) {
  Reset();
}

void BookmarkEditorGtk::BookmarkNodeRemoved(BookmarkModel* model,
                                            const BookmarkNode* parent,
                                            int index,
                                            const BookmarkNode* node) {
  if ((details_.type == EditDetails::EXISTING_NODE &&
       details_.existing_node->HasAncestor(node)) ||
      (parent_ && parent_->HasAncestor(node))) {
    // The node, or its parent was removed. Close the dialog.
    Close();
  } else {
    Reset();
  }
}

void BookmarkEditorGtk::BookmarkNodeChildrenReordered(
    BookmarkModel* model, const BookmarkNode* node) {
  Reset();
}

void BookmarkEditorGtk::Reset() {
  // TODO(erg): The windows implementation tries to be smart. For now, just
  // close the window.
  Close();
}

GURL BookmarkEditorGtk::GetInputURL() const {
  if (!url_entry_)
    return GURL();  // Happens when we're editing a folder.
  return URLFixerUpper::FixupURL(gtk_entry_get_text(GTK_ENTRY(url_entry_)),
                                 std::string());
}

string16 BookmarkEditorGtk::GetInputTitle() const {
  return UTF8ToUTF16(gtk_entry_get_text(GTK_ENTRY(name_entry_)));
}

void BookmarkEditorGtk::ApplyEdits() {
  DCHECK(bb_model_->IsLoaded());

  GtkTreeIter currently_selected_iter;
  if (show_tree_) {
    if (!gtk_tree_selection_get_selected(tree_selection_, NULL,
                                         &currently_selected_iter)) {
      ApplyEdits(NULL);
      return;
    }
  }

  ApplyEdits(&currently_selected_iter);
}

void BookmarkEditorGtk::ApplyEdits(GtkTreeIter* selected_parent) {
  // We're going to apply edits to the bookmark bar model, which will call us
  // back. Normally when a structural edit occurs we reset the tree model.
  // We don't want to do that here, so we remove ourselves as an observer.
  bb_model_->RemoveObserver(this);

  GURL new_url(GetInputURL());
  string16 new_title(GetInputTitle());

  if (!show_tree_ || !selected_parent) {
    bookmark_utils::ApplyEditsWithNoGroupChange(
        bb_model_, parent_, details_, new_title, new_url);
    return;
  }

  // Create the new groups and update the titles.
  const BookmarkNode* new_parent =
      bookmark_utils::CommitTreeStoreDifferencesBetween(
      bb_model_, tree_store_, selected_parent);

  if (!new_parent) {
    // Bookmarks must be parented.
    NOTREACHED();
    return;
  }

  bookmark_utils::ApplyEditsWithPossibleGroupChange(
      bb_model_, new_parent, details_, new_title, new_url);
}

void BookmarkEditorGtk::AddNewGroup(GtkTreeIter* parent, GtkTreeIter* child) {
  gtk_tree_store_append(tree_store_, child, parent);
  gtk_tree_store_set(
      tree_store_, child,
      bookmark_utils::FOLDER_ICON, GtkThemeProvider::GetFolderIcon(true),
      bookmark_utils::FOLDER_NAME,
          l10n_util::GetStringUTF8(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME).c_str(),
      bookmark_utils::ITEM_ID, static_cast<int64>(0),
      bookmark_utils::IS_EDITABLE, TRUE,
      -1);
}

void BookmarkEditorGtk::OnSelectionChanged(GtkWidget* selection) {
  if (!gtk_tree_selection_get_selected(tree_selection_, NULL, NULL))
    gtk_widget_set_sensitive(new_folder_button_, FALSE);
  else
    gtk_widget_set_sensitive(new_folder_button_, TRUE);
}

void BookmarkEditorGtk::OnResponse(GtkWidget* dialog, int response_id) {
  if (response_id == GTK_RESPONSE_ACCEPT)
    ApplyEdits();

  Close();
}

gboolean BookmarkEditorGtk::OnWindowDeleteEvent(GtkWidget* widget,
                                                GdkEvent* event) {
  Close();

  // Return true to prevent the gtk dialog from being destroyed. Close will
  // destroy it for us and the default gtk_dialog_delete_event_handler() will
  // force the destruction without us being able to stop it.
  return TRUE;
}

void BookmarkEditorGtk::OnWindowDestroy(GtkWidget* widget) {
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void BookmarkEditorGtk::OnEntryChanged(GtkWidget* entry) {
  gboolean can_close = TRUE;
  if (details_.type == EditDetails::NEW_FOLDER) {
    if (GetInputTitle().empty()) {
      gtk_widget_modify_base(name_entry_, GTK_STATE_NORMAL,
                             &kErrorColor);
      can_close = FALSE;
    } else {
      gtk_widget_modify_base(name_entry_, GTK_STATE_NORMAL, NULL);
    }
  } else {
    GURL url(GetInputURL());
    if (!url.is_valid()) {
      gtk_widget_modify_base(url_entry_, GTK_STATE_NORMAL,
                             &kErrorColor);
      can_close = FALSE;
    } else {
      gtk_widget_modify_base(url_entry_, GTK_STATE_NORMAL, NULL);
    }
  }
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog_),
                                    GTK_RESPONSE_ACCEPT, can_close);
}

void BookmarkEditorGtk::OnNewFolderClicked(GtkWidget* button) {
  NewFolder();
}

gboolean BookmarkEditorGtk::OnTreeViewButtonPressEvent(GtkWidget* widget,
                                                       GdkEventButton* event) {
  if (event->button == 3) {
    gfx::Point pt(event->x_root, event->y_root);
    ShowContextMenu(pt);
  }

  return FALSE;
}

void BookmarkEditorGtk::ShowContextMenu(const gfx::Point& point) {
  if (!menu_controller_.get())
    menu_controller_.reset(new ContextMenuController(this));

  menu_controller_->RunMenu(point);
}

void BookmarkEditorGtk::NewFolder() {
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(tree_selection_,
                                       NULL,
                                       &iter)) {
    NOTREACHED() << "Something should always be selected if New Folder " <<
                    "is clicked";
    return;
  }

  GtkTreeIter new_item_iter;
  AddNewGroup(&iter, &new_item_iter);

  GtkTreePath* path = gtk_tree_model_get_path(
      GTK_TREE_MODEL(tree_store_), &new_item_iter);
  gtk_tree_view_expand_to_path(GTK_TREE_VIEW(tree_view_), path);

  // Make the folder name editable.
  gtk_tree_view_set_cursor(GTK_TREE_VIEW(tree_view_), path,
      gtk_tree_view_get_column(GTK_TREE_VIEW(tree_view_), 0),
      TRUE);

  gtk_tree_path_free(path);
}
