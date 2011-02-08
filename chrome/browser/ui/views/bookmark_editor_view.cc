// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmark_editor_view.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/background.h"
#include "views/focus/focus_manager.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/controls/menu/menu_2.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

using views::Button;
using views::ColumnSet;
using views::GridLayout;
using views::Label;
using views::NativeButton;
using views::Textfield;

// Background color of text field when URL is invalid.
static const SkColor kErrorColor = SkColorSetRGB(0xFF, 0xBC, 0xBC);

// Preferred width of the tree.
static const int kTreeWidth = 300;

// ID for various children.
static const int kNewGroupButtonID = 1002;

// static
void BookmarkEditor::Show(HWND parent_hwnd,
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
      new_group_button_(NULL),
      url_label_(NULL),
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

bool BookmarkEditorView::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
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

std::wstring BookmarkEditorView::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_BOOMARK_EDITOR_TITLE));
}

bool BookmarkEditorView::Accept() {
  if (!IsDialogButtonEnabled(MessageBoxFlags::DIALOGBUTTON_OK)) {
    // The url is invalid, focus the url field.
    url_tf_.SelectAll();
    url_tf_.RequestFocus();
    return false;
  }
  // Otherwise save changes and close the dialog box.
  ApplyEdits();
  return true;
}

bool BookmarkEditorView::AreAcceleratorsEnabled(
    MessageBoxFlags::DialogButton button) {
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
  gfx::Rect parent_bounds = GetParent()->GetLocalBounds();
  gfx::Size prefsize = new_group_button_->GetPreferredSize();
  int button_y =
      parent_bounds.bottom() - prefsize.height() - kButtonVEdgeMargin;
  new_group_button_->SetBounds(kPanelHorizMargin, button_y, prefsize.width(),
                              prefsize.height());
}

gfx::Size BookmarkEditorView::GetPreferredSize() {
  if (!show_tree_)
    return views::View::GetPreferredSize();

  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_EDITBOOKMARK_DIALOG_WIDTH_CHARS,
      IDS_EDITBOOKMARK_DIALOG_HEIGHT_LINES));
}

void BookmarkEditorView::ViewHierarchyChanged(bool is_add,
                                              views::View* parent,
                                              views::View* child) {
  if (show_tree_ && child == this) {
    // Add and remove the New Folder button from the ClientView's hierarchy.
    if (is_add) {
      parent->AddChildView(new_group_button_.get());
    } else {
      parent->RemoveChildView(new_group_button_.get());
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
  return (bb_node->GetParent() && bb_node->GetParent()->GetParent());
}

void BookmarkEditorView::ContentsChanged(Textfield* sender,
                                         const std::wstring& new_contents) {
  UserInputChanged();
}

void BookmarkEditorView::ButtonPressed(
    Button* sender, const views::Event& event) {
  DCHECK(sender);
  switch (sender->GetID()) {
    case kNewGroupButtonID:
      NewGroup();
      break;

    default:
      NOTREACHED();
  }
}

bool BookmarkEditorView::IsCommandIdChecked(int command_id) const {
  return false;
}

bool BookmarkEditorView::IsCommandIdEnabled(int command_id) const {
  return (command_id != IDS_EDIT || !running_menu_for_root_);
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
  } else {
    DCHECK(command_id == IDS_BOOMARK_EDITOR_NEW_FOLDER_MENU_ITEM);
    NewGroup();
  }
}

void BookmarkEditorView::Show(HWND parent_hwnd) {
  views::Window::CreateChromeWindow(parent_hwnd, gfx::Rect(), this);
  UserInputChanged();
  if (show_tree_ && bb_model_->IsLoaded())
    ExpandAndSelect();
  window()->Show();
  // Select all the text in the name Textfield.
  title_tf_.SelectAll();
  // Give focus to the name Textfield.
  title_tf_.RequestFocus();
}

void BookmarkEditorView::Close() {
  DCHECK(window());
  window()->Close();
}

void BookmarkEditorView::ShowContextMenu(View* source,
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
    context_menu_contents_->AddItemWithStringId(
        IDS_BOOMARK_EDITOR_NEW_FOLDER_MENU_ITEM,
        IDS_BOOMARK_EDITOR_NEW_FOLDER_MENU_ITEM);
    context_menu_.reset(new views::Menu2(context_menu_contents_.get()));
  }
  context_menu_->RunContextMenuAt(p);
}

void BookmarkEditorView::Init() {
  bb_model_ = profile_->GetBookmarkModel();
  DCHECK(bb_model_);
  bb_model_->AddObserver(this);

  url_tf_.set_parent_owned(false);
  title_tf_.set_parent_owned(false);

  std::wstring title;
  if (details_.type == EditDetails::EXISTING_NODE)
    title = details_.existing_node->GetTitle();
  else if (details_.type == EditDetails::NEW_FOLDER)
    title = UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME));
  title_tf_.SetText(title);
  title_tf_.SetController(this);

  title_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_BOOMARK_EDITOR_NAME_LABEL)));
  title_tf_.SetAccessibleName(WideToUTF16Hack(title_label_->GetText()));

  string16 url_text;
  if (details_.type == EditDetails::EXISTING_NODE) {
    std::string languages = profile_
        ? profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)
        : std::string();
    // Because this gets parsed by FixupURL(), it's safe to omit the scheme or
    // trailing slash, and unescape most characters, but we need to not drop any
    // username/password, or unescape anything that changes the meaning.
    url_text = net::FormatUrl(details_.existing_node->GetURL(), languages,
        net::kFormatUrlOmitAll & ~net::kFormatUrlOmitUsernamePassword,
        UnescapeRule::SPACES, NULL, NULL, NULL);
  }
  url_tf_.SetText(UTF16ToWide(url_text));
  url_tf_.SetController(this);

  url_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_BOOMARK_EDITOR_URL_LABEL)));
  url_tf_.SetAccessibleName(WideToUTF16Hack(url_label_->GetText()));

  if (show_tree_) {
    tree_view_ = new views::TreeView();
    new_group_button_.reset(new views::NativeButton(
        this,
        UTF16ToWide(l10n_util::GetStringUTF16(
            IDS_BOOMARK_EDITOR_NEW_FOLDER_BUTTON))));
    new_group_button_->set_parent_owned(false);
    tree_view_->SetContextMenuController(this);

    tree_view_->SetRootShown(false);
    new_group_button_->SetEnabled(false);
    new_group_button_->SetID(kNewGroupButtonID);
  }

  // Yummy layout code.
  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int labels_column_set_id = 0;
  const int single_column_view_set_id = 1;
  const int buttons_column_set_id = 2;

  ColumnSet* column_set = layout->AddColumnSet(labels_column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, kTreeWidth, 0);

  column_set = layout->AddColumnSet(buttons_column_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(1, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->LinkColumnSizes(0, 2, 4, -1);

  layout->StartRow(0, labels_column_set_id);

  layout->AddView(title_label_);
  layout->AddView(&title_tf_);

  if (details_.type != EditDetails::NEW_FOLDER) {
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

    layout->StartRow(0, labels_column_set_id);
    layout->AddView(url_label_);
    layout->AddView(&url_tf_);
  }

  if (show_tree_) {
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    layout->StartRow(1, single_column_view_set_id);
    layout->AddView(tree_view_);
  }

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

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
    window()->Close();
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
    if (GetParent())
      UserInputChanged();
    return;
  }

  new_group_button_->SetEnabled(true);

  // Do this first, otherwise when we invoke SetModel with the real one
  // tree_view will try to invoke something on the model we just deleted.
  tree_view_->SetModel(NULL);

  EditorNode* root_node = CreateRootNode();
  tree_model_.reset(new EditorTreeModel(root_node));

  tree_view_->SetModel(tree_model_.get());
  tree_view_->SetController(this);

  context_menu_.reset();

  if (GetParent())
    ExpandAndSelect();
}
GURL BookmarkEditorView::GetInputURL() const {
  return URLFixerUpper::FixupURL(UTF16ToUTF8(url_tf_.text()), std::string());
}

std::wstring BookmarkEditorView::GetInputTitle() const {
  return title_tf_.text();
}

void BookmarkEditorView::UserInputChanged() {
  const GURL url(GetInputURL());
  if (!url.is_valid())
    url_tf_.SetBackgroundColor(kErrorColor);
  else
    url_tf_.UseDefaultBackgroundColor();
  GetDialogClientView()->UpdateDialogButtons();
}

void BookmarkEditorView::NewGroup() {
  // Create a new entry parented to the selected item, or the bookmark
  // bar if nothing is selected.
  EditorNode* parent = tree_model_->AsNode(tree_view_->GetSelectedNode());
  if (!parent) {
    NOTREACHED();
    return;
  }

  tree_view_->StartEditing(AddNewGroup(parent));
}

BookmarkEditorView::EditorNode* BookmarkEditorView::AddNewGroup(
    EditorNode* parent) {
  EditorNode* new_node = new EditorNode();
  new_node->SetTitle(
      l10n_util::GetStringUTF16(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME));
  new_node->value = 0;
  // new_node is now owned by parent.
  tree_model_->Add(parent, parent->GetChildCount(), new_node);
  return new_node;
}

void BookmarkEditorView::ExpandAndSelect() {
  tree_view_->ExpandAll();

  const BookmarkNode* to_select = parent_;
  if (details_.type == EditDetails::EXISTING_NODE)
    to_select = details_.existing_node->GetParent();
  int64 group_id_to_select = to_select->id();
  EditorNode* b_node =
      FindNodeWithID(tree_model_->GetRoot(), group_id_to_select);
  if (!b_node)
    b_node = tree_model_->GetRoot()->GetChild(0);  // Bookmark bar node.

  tree_view_->SetSelectedNode(b_node);
}

BookmarkEditorView::EditorNode* BookmarkEditorView::CreateRootNode() {
  EditorNode* root_node = new EditorNode(std::wstring(), 0);
  const BookmarkNode* bb_root_node = bb_model_->root_node();
  CreateNodes(bb_root_node, root_node);
  DCHECK(root_node->GetChildCount() == 2);
  DCHECK(bb_root_node->GetChild(0)->type() == BookmarkNode::BOOKMARK_BAR);
  DCHECK(bb_root_node->GetChild(1)->type() == BookmarkNode::OTHER_NODE);
  return root_node;
}

void BookmarkEditorView::CreateNodes(const BookmarkNode* bb_node,
                                     BookmarkEditorView::EditorNode* b_node) {
  for (int i = 0; i < bb_node->GetChildCount(); ++i) {
    const BookmarkNode* child_bb_node = bb_node->GetChild(i);
    if (child_bb_node->is_folder()) {
      EditorNode* new_b_node =
          new EditorNode(WideToUTF16(child_bb_node->GetTitle()),
                                     child_bb_node->id());
      b_node->Add(b_node->GetChildCount(), new_b_node);
      CreateNodes(child_bb_node, new_b_node);
    }
  }
}

BookmarkEditorView::EditorNode* BookmarkEditorView::FindNodeWithID(
    BookmarkEditorView::EditorNode* node,
    int64 id) {
  if (node->value == id)
    return node;
  for (int i = 0; i < node->GetChildCount(); ++i) {
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
  string16 new_title(WideToUTF16Hack(GetInputTitle()));

  if (!show_tree_) {
    bookmark_utils::ApplyEditsWithNoGroupChange(
        bb_model_, parent_, details_, new_title, new_url);
    return;
  }

  // Create the new groups and update the titles.
  const BookmarkNode* new_parent = NULL;
  ApplyNameChangesAndCreateNewGroups(
      bb_model_->root_node(), tree_model_->GetRoot(), parent, &new_parent);

  bookmark_utils::ApplyEditsWithPossibleGroupChange(
      bb_model_, new_parent, details_, new_title, new_url);
}

void BookmarkEditorView::ApplyNameChangesAndCreateNewGroups(
    const BookmarkNode* bb_node,
    BookmarkEditorView::EditorNode* b_node,
    BookmarkEditorView::EditorNode* parent_b_node,
    const BookmarkNode** parent_bb_node) {
  if (parent_b_node == b_node)
    *parent_bb_node = bb_node;
  for (int i = 0; i < b_node->GetChildCount(); ++i) {
    EditorNode* child_b_node = b_node->GetChild(i);
    const BookmarkNode* child_bb_node = NULL;
    if (child_b_node->value == 0) {
      // New group.
      child_bb_node = bb_model_->AddGroup(bb_node,
          bb_node->GetChildCount(), child_b_node->GetTitle());
    } else {
      // Existing node, reset the title (BBModel ignores changes if the title
      // is the same).
      for (int j = 0; j < bb_node->GetChildCount(); ++j) {
        const BookmarkNode* node = bb_node->GetChild(j);
        if (node->is_folder() && node->id() == child_b_node->value) {
          child_bb_node = node;
          break;
        }
      }
      DCHECK(child_bb_node);
      bb_model_->SetTitle(child_bb_node, child_b_node->GetTitle());
    }
    ApplyNameChangesAndCreateNewGroups(child_bb_node, child_b_node,
                                       parent_b_node, parent_bb_node);
  }
}
