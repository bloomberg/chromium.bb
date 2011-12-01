// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_2.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

using views::GridLayout;

namespace {

// Background color of text field when URL is invalid.
const SkColor kErrorColor = SkColorSetRGB(0xFF, 0xBC, 0xBC);

// Preferred width of the tree.
const int kTreeWidth = 300;

// ID for various children.
const int kNewFolderButtonID = 1002;

}  // namespace

// static
void BookmarkEditor::ShowNative(gfx::NativeWindow parent_hwnd,
                                Profile* profile,
                                const BookmarkNode* parent,
                                const EditDetails& details,
                                Configuration configuration) {
  DCHECK(profile);
  BookmarkEditorView* editor =
      new BookmarkEditorView(profile, parent, details, configuration);
  editor->Show(parent_hwnd);
}

BookmarkEditorView::BookmarkEditorView(
    Profile* profile,
    const BookmarkNode* parent,
    const EditDetails& details,
    BookmarkEditor::Configuration configuration)
    : profile_(profile),
      tree_view_(NULL),
      new_folder_button_(NULL),
      url_label_(NULL),
      url_tf_(NULL),
      title_label_(NULL),
      parent_(parent),
      details_(details),
      running_menu_for_root_(false),
      show_tree_(configuration == SHOW_TREE) {
  DCHECK(profile);
  Init();
}

BookmarkEditorView::~BookmarkEditorView() {
  // The tree model is deleted before the view. Reset the model otherwise the
  // tree will reference a deleted model.
  if (tree_view_)
    tree_view_->SetModel(NULL);
  bb_model_->RemoveObserver(this);
}

string16 BookmarkEditorView::GetDialogButtonLabel(
      ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_SAVE);
  return string16();
}
bool BookmarkEditorView::IsDialogButtonEnabled(ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    if (details_.type == EditDetails::NEW_FOLDER)
      return !title_tf_.text().empty();

    const GURL url(GetInputURL());
    return bb_model_->IsLoaded() && url.is_valid();
  }
  return true;
}

bool BookmarkEditorView::IsModal() const {
  return true;
}

bool BookmarkEditorView::CanResize() const {
  return true;
}

string16 BookmarkEditorView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_TITLE);
}

bool BookmarkEditorView::Accept() {
  if (!IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK)) {
    if (details_.type != EditDetails::NEW_FOLDER) {
      // The url is invalid, focus the url field.
      url_tf_->SelectAll();
      url_tf_->RequestFocus();
    }
    return false;
  }
  // Otherwise save changes and close the dialog box.
  ApplyEdits();
  return true;
}

bool BookmarkEditorView::AreAcceleratorsEnabled(ui::DialogButton button) {
  return !show_tree_ || !tree_view_->GetEditingNode();
}

views::View* BookmarkEditorView::GetContentsView() {
  return this;
}

void BookmarkEditorView::Layout() {
  // Let the grid layout manager lay out most of the dialog...
  GetLayoutManager()->Layout(this);

  if (!show_tree_)
    return;

  // Manually lay out the New Folder button in the same row as the OK/Cancel
  // buttons...
  gfx::Rect parent_bounds = parent()->GetContentsBounds();
  gfx::Size prefsize = new_folder_button_->GetPreferredSize();
  int button_y =
      parent_bounds.bottom() - prefsize.height() - views::kButtonVEdgeMargin;
  new_folder_button_->SetBounds(
      views::kPanelHorizMargin, button_y, prefsize.width(), prefsize.height());
}

gfx::Size BookmarkEditorView::GetPreferredSize() {
  if (!show_tree_)
    return views::View::GetPreferredSize();

  return gfx::Size(views::Widget::GetLocalizedContentsSize(
      IDS_EDITBOOKMARK_DIALOG_WIDTH_CHARS,
      IDS_EDITBOOKMARK_DIALOG_HEIGHT_LINES));
}

void BookmarkEditorView::ViewHierarchyChanged(bool is_add,
                                              views::View* parent,
                                              views::View* child) {
  if (show_tree_ && child == this) {
    // Add and remove the New Folder button from the ClientView's hierarchy.
    if (is_add) {
      parent->AddChildView(new_folder_button_.get());
    } else {
      parent->RemoveChildView(new_folder_button_.get());
    }
  }
}

void BookmarkEditorView::OnTreeViewSelectionChanged(
    views::TreeView* tree_view) {
}

bool BookmarkEditorView::CanEdit(views::TreeView* tree_view,
                                 ui::TreeModelNode* node) {
  // Only allow editting of children of the bookmark bar node and other node.
  EditorNode* bb_node = tree_model_->AsNode(node);
  return (bb_node->parent() && bb_node->parent()->parent());
}

void BookmarkEditorView::ContentsChanged(views::Textfield* sender,
                                         const string16& new_contents) {
  UserInputChanged();
}

void BookmarkEditorView::ButtonPressed(views::Button* sender,
                                       const views::Event& event) {
  DCHECK(sender);
  switch (sender->id()) {
    case kNewFolderButtonID:
      NewFolder();
      break;

    default:
      NOTREACHED();
  }
}

bool BookmarkEditorView::IsCommandIdChecked(int command_id) const {
  return false;
}

bool BookmarkEditorView::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case IDS_EDIT:
    case IDS_DELETE:
      return !running_menu_for_root_;
    case IDS_BOOKMARK_EDITOR_NEW_FOLDER_MENU_ITEM:
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool BookmarkEditorView::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return GetWidget()->GetAccelerator(command_id, accelerator);
}

void BookmarkEditorView::ExecuteCommand(int command_id) {
  DCHECK(tree_view_->GetSelectedNode());
  if (command_id == IDS_EDIT) {
    tree_view_->StartEditing(tree_view_->GetSelectedNode());
  } else if (command_id == IDS_DELETE) {
    EditorNode* node = tree_model_->AsNode(tree_view_->GetSelectedNode());
    if (!node)
      return;
    if (node->value != 0) {
      const BookmarkNode* b_node = bb_model_->GetNodeByID(node->value);
      if (!b_node->empty() &&
          !bookmark_utils::ConfirmDeleteBookmarkNode(b_node,
            GetWidget()->GetNativeWindow())) {
        // The folder is not empty and the user didn't confirm.
        return;
      }
      deletes_.push_back(node->value);
    }
    tree_model_->Remove(node->parent(), node);
  } else {
    DCHECK(command_id == IDS_BOOKMARK_EDITOR_NEW_FOLDER_MENU_ITEM);
    NewFolder();
  }
}

void BookmarkEditorView::Show(gfx::NativeWindow parent_hwnd) {
  views::Widget::CreateWindowWithParent(this, parent_hwnd);
  UserInputChanged();
  if (show_tree_ && bb_model_->IsLoaded())
    ExpandAndSelect();
  GetWidget()->Show();
  // Select all the text in the name Textfield.
  title_tf_.SelectAll();
  // Give focus to the name Textfield.
  title_tf_.RequestFocus();
}

void BookmarkEditorView::Close() {
  DCHECK(GetWidget());
  GetWidget()->Close();
}

void BookmarkEditorView::ShowContextMenuForView(View* source,
                                                const gfx::Point& p,
                                                bool is_mouse_gesture) {
  DCHECK(source == tree_view_);
  if (!tree_view_->GetSelectedNode())
    return;
  running_menu_for_root_ =
      (tree_model_->GetParent(tree_view_->GetSelectedNode()) ==
       tree_model_->GetRoot());
  if (!context_menu_contents_.get()) {
    context_menu_contents_.reset(new ui::SimpleMenuModel(this));
    context_menu_contents_->AddItemWithStringId(IDS_EDIT, IDS_EDIT);
    context_menu_contents_->AddItemWithStringId(IDS_DELETE, IDS_DELETE);
    context_menu_contents_->AddItemWithStringId(
        IDS_BOOKMARK_EDITOR_NEW_FOLDER_MENU_ITEM,
        IDS_BOOKMARK_EDITOR_NEW_FOLDER_MENU_ITEM);
    context_menu_.reset(new views::Menu2(context_menu_contents_.get()));
  }
  context_menu_->RunContextMenuAt(p);
}

void BookmarkEditorView::Init() {
  bb_model_ = profile_->GetBookmarkModel();
  DCHECK(bb_model_);
  bb_model_->AddObserver(this);

  title_tf_.set_parent_owned(false);

  std::wstring title;
  GURL url;
  if (details_.type == EditDetails::EXISTING_NODE) {
    title = details_.existing_node->GetTitle();
    url = details_.existing_node->url();
  } else if (details_.type == EditDetails::NEW_FOLDER) {
    title = UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NEW_FOLDER_NAME));
  } else if (details_.type == EditDetails::NEW_URL) {
    string16 title16;
    bookmark_utils::GetURLAndTitleToBookmarkFromCurrentTab(profile_,
        &url, &title16);
    title = UTF16ToWide(title16);
  }
  title_tf_.SetText(title);
  title_tf_.SetController(this);

  title_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NAME_LABEL));
  title_tf_.SetAccessibleName(title_label_->GetText());

  if (show_tree_) {
    tree_view_ = new views::TreeView();
    tree_view_->set_lines_at_root(true);
    new_folder_button_.reset(new views::NativeTextButton(
        this,
        UTF16ToWide(l10n_util::GetStringUTF16(
            IDS_BOOKMARK_EDITOR_NEW_FOLDER_BUTTON))));
    new_folder_button_->set_parent_owned(false);
    tree_view_->set_context_menu_controller(this);

    tree_view_->SetRootShown(false);
    new_folder_button_->SetEnabled(false);
    new_folder_button_->set_id(kNewFolderButtonID);
  }

  // Yummy layout code.
  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int labels_column_set_id = 0;
  const int single_column_view_set_id = 1;
  const int buttons_column_set_id = 2;

  views::ColumnSet* column_set = layout->AddColumnSet(labels_column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, kTreeWidth, 0);

  column_set = layout->AddColumnSet(buttons_column_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(1, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->LinkColumnSizes(0, 2, 4, -1);

  layout->StartRow(0, labels_column_set_id);

  layout->AddView(title_label_);
  layout->AddView(&title_tf_);

  if (details_.type != EditDetails::NEW_FOLDER) {
    url_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_URL_LABEL));

    std::string languages =
        profile_ ? profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)
                 : std::string();
    // Because this gets parsed by FixupURL(), it's safe to omit the scheme or
    // trailing slash, and unescape most characters, but we need to not drop any
    // username/password, or unescape anything that changes the meaning.
    string16 url_text = net::FormatUrl(url, languages,
        net::kFormatUrlOmitAll & ~net::kFormatUrlOmitUsernamePassword,
        net::UnescapeRule::SPACES, NULL, NULL, NULL);

    url_tf_ = new views::Textfield;
    url_tf_->SetText(UTF16ToWide(url_text));
    url_tf_->SetController(this);
    url_tf_->SetAccessibleName(url_label_->GetText());

    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    layout->StartRow(0, labels_column_set_id);
    layout->AddView(url_label_);
    layout->AddView(url_tf_);
  }

  if (show_tree_) {
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    layout->StartRow(1, single_column_view_set_id);
    layout->AddView(tree_view_);
  }

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  if (!show_tree_ || bb_model_->IsLoaded())
    Reset();
}

void BookmarkEditorView::BookmarkNodeMoved(BookmarkModel* model,
                                           const BookmarkNode* old_parent,
                                           int old_index,
                                           const BookmarkNode* new_parent,
                                           int new_index) {
  Reset();
}

void BookmarkEditorView::BookmarkNodeAdded(BookmarkModel* model,
                                           const BookmarkNode* parent,
                                           int index) {
  Reset();
}

void BookmarkEditorView::BookmarkNodeRemoved(BookmarkModel* model,
                                             const BookmarkNode* parent,
                                             int index,
                                             const BookmarkNode* node) {
  if ((details_.type == EditDetails::EXISTING_NODE &&
       details_.existing_node->HasAncestor(node)) ||
      (parent_ && parent_->HasAncestor(node))) {
    // The node, or its parent was removed. Close the dialog.
    GetWidget()->Close();
  } else {
    Reset();
  }
}

void BookmarkEditorView::BookmarkNodeChildrenReordered(
    BookmarkModel* model, const BookmarkNode* node) {
  Reset();
}

void BookmarkEditorView::Reset() {
  if (!show_tree_) {
    if (parent())
      UserInputChanged();
    return;
  }

  new_folder_button_->SetEnabled(true);

  // Do this first, otherwise when we invoke SetModel with the real one
  // tree_view will try to invoke something on the model we just deleted.
  tree_view_->SetModel(NULL);

  EditorNode* root_node = CreateRootNode();
  tree_model_.reset(new EditorTreeModel(root_node));

  tree_view_->SetModel(tree_model_.get());
  tree_view_->SetController(this);

  context_menu_.reset();

  if (parent())
    ExpandAndSelect();
}

GURL BookmarkEditorView::GetInputURL() const {
  if (details_.type == EditDetails::NEW_FOLDER)
    return GURL();
  return URLFixerUpper::FixupURL(UTF16ToUTF8(url_tf_->text()), std::string());
}

string16 BookmarkEditorView::GetInputTitle() const {
  return title_tf_.text();
}

void BookmarkEditorView::UserInputChanged() {
  if (details_.type != EditDetails::NEW_FOLDER) {
    const GURL url(GetInputURL());
    if (!url.is_valid())
      url_tf_->SetBackgroundColor(kErrorColor);
    else
      url_tf_->UseDefaultBackgroundColor();
  }
  GetDialogClientView()->UpdateDialogButtons();
}

void BookmarkEditorView::NewFolder() {
  // Create a new entry parented to the selected item, or the bookmark
  // bar if nothing is selected.
  EditorNode* parent = tree_model_->AsNode(tree_view_->GetSelectedNode());
  if (!parent) {
    NOTREACHED();
    return;
  }

  tree_view_->StartEditing(AddNewFolder(parent));
}

BookmarkEditorView::EditorNode* BookmarkEditorView::AddNewFolder(
    EditorNode* parent) {
  EditorNode* new_node = new EditorNode(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NEW_FOLDER_NAME), 0);
  // |new_node| is now owned by |parent|.
  tree_model_->Add(parent, new_node, parent->child_count());
  return new_node;
}

void BookmarkEditorView::ExpandAndSelect() {
  BookmarkExpandedStateTracker::Nodes expanded_nodes =
      bb_model_->expanded_state_tracker()->GetExpandedNodes();
  for (BookmarkExpandedStateTracker::Nodes::const_iterator i =
       expanded_nodes.begin(); i != expanded_nodes.end(); ++i) {
    EditorNode* editor_node =
        FindNodeWithID(tree_model_->GetRoot(), (*i)->id());
    if (editor_node)
      tree_view_->Expand(editor_node);
  }

  const BookmarkNode* to_select = parent_;
  if (details_.type == EditDetails::EXISTING_NODE)
    to_select = details_.existing_node->parent();
  int64 folder_id_to_select = to_select->id();
  EditorNode* b_node =
      FindNodeWithID(tree_model_->GetRoot(), folder_id_to_select);
  if (!b_node)
    b_node = tree_model_->GetRoot()->GetChild(0);  // Bookmark bar node.

  tree_view_->SetSelectedNode(b_node);
}

BookmarkEditorView::EditorNode* BookmarkEditorView::CreateRootNode() {
  EditorNode* root_node = new EditorNode(string16(), 0);
  const BookmarkNode* bb_root_node = bb_model_->root_node();
  CreateNodes(bb_root_node, root_node);
  DCHECK(root_node->child_count() >= 2 && root_node->child_count() <= 3);
  DCHECK(bb_root_node->GetChild(0)->type() == BookmarkNode::BOOKMARK_BAR);
  DCHECK(bb_root_node->GetChild(1)->type() == BookmarkNode::OTHER_NODE);
  if (root_node->child_count() == 3) {
    DCHECK(bb_root_node->GetChild(2)->type() == BookmarkNode::SYNCED);
  }
  return root_node;
}

void BookmarkEditorView::CreateNodes(const BookmarkNode* bb_node,
                                     BookmarkEditorView::EditorNode* b_node) {
  for (int i = 0; i < bb_node->child_count(); ++i) {
    const BookmarkNode* child_bb_node = bb_node->GetChild(i);
    if (child_bb_node->IsVisible() && child_bb_node->is_folder()) {
      EditorNode* new_b_node = new EditorNode(child_bb_node->GetTitle(),
                                              child_bb_node->id());
      b_node->Add(new_b_node, b_node->child_count());
      CreateNodes(child_bb_node, new_b_node);
    }
  }
}

BookmarkEditorView::EditorNode* BookmarkEditorView::FindNodeWithID(
    BookmarkEditorView::EditorNode* node,
    int64 id) {
  if (node->value == id)
    return node;
  for (int i = 0; i < node->child_count(); ++i) {
    EditorNode* result = FindNodeWithID(node->GetChild(i), id);
    if (result)
      return result;
  }
  return NULL;
}

void BookmarkEditorView::ApplyEdits() {
  DCHECK(bb_model_->IsLoaded());

  EditorNode* parent = show_tree_ ?
      tree_model_->AsNode(tree_view_->GetSelectedNode()) : NULL;
  if (show_tree_ && !parent) {
    NOTREACHED();
    return;
  }
  ApplyEdits(parent);
}

void BookmarkEditorView::ApplyEdits(EditorNode* parent) {
  DCHECK(!show_tree_ || parent);

  // We're going to apply edits to the bookmark bar model, which will call us
  // back. Normally when a structural edit occurs we reset the tree model.
  // We don't want to do that here, so we remove ourselves as an observer.
  bb_model_->RemoveObserver(this);

  GURL new_url(GetInputURL());
  string16 new_title(GetInputTitle());

  if (!show_tree_) {
    bookmark_utils::ApplyEditsWithNoFolderChange(
        bb_model_, parent_, details_, new_title, new_url);
    return;
  }

  // Create the new folders and update the titles.
  const BookmarkNode* new_parent = NULL;
  ApplyNameChangesAndCreateNewFolders(
      bb_model_->root_node(), tree_model_->GetRoot(), parent, &new_parent);

  bookmark_utils::ApplyEditsWithPossibleFolderChange(
      bb_model_, new_parent, details_, new_title, new_url);

  BookmarkExpandedStateTracker::Nodes expanded_nodes;
  UpdateExpandedNodes(tree_model_->GetRoot(), &expanded_nodes);
  bb_model_->expanded_state_tracker()->SetExpandedNodes(expanded_nodes);

  // Remove the folders that were removed. This has to be done after all the
  // other changes have been committed.
  bookmark_utils::DeleteBookmarkFolders(bb_model_, deletes_);
}

void BookmarkEditorView::ApplyNameChangesAndCreateNewFolders(
    const BookmarkNode* bb_node,
    BookmarkEditorView::EditorNode* b_node,
    BookmarkEditorView::EditorNode* parent_b_node,
    const BookmarkNode** parent_bb_node) {
  if (parent_b_node == b_node)
    *parent_bb_node = bb_node;
  for (int i = 0; i < b_node->child_count(); ++i) {
    EditorNode* child_b_node = b_node->GetChild(i);
    const BookmarkNode* child_bb_node = NULL;
    if (child_b_node->value == 0) {
      // New folder.
      child_bb_node = bb_model_->AddFolder(bb_node,
          bb_node->child_count(), child_b_node->GetTitle());
      child_b_node->value = child_bb_node->id();
    } else {
      // Existing node, reset the title (BookmarkModel ignores changes if the
      // title is the same).
      for (int j = 0; j < bb_node->child_count(); ++j) {
        const BookmarkNode* node = bb_node->GetChild(j);
        if (node->is_folder() && node->id() == child_b_node->value) {
          child_bb_node = node;
          break;
        }
      }
      DCHECK(child_bb_node);
      bb_model_->SetTitle(child_bb_node, child_b_node->GetTitle());
    }
    ApplyNameChangesAndCreateNewFolders(child_bb_node, child_b_node,
                                        parent_b_node, parent_bb_node);
  }
}

void BookmarkEditorView::UpdateExpandedNodes(
    EditorNode* editor_node,
    BookmarkExpandedStateTracker::Nodes* expanded_nodes) {
  if (!tree_view_->IsExpanded(editor_node))
    return;

  if (editor_node->value != 0)  // The root is 0
    expanded_nodes->insert(bb_model_->GetNodeByID(editor_node->value));
  for (int i = 0; i < editor_node->child_count(); ++i)
    UpdateExpandedNodes(editor_node->GetChild(i), expanded_nodes);
}
