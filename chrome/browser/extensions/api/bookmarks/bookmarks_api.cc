// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/time_formatting.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/sha1.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_html_writer.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/extensions/api/bookmarks/bookmark_api_constants.h"
#include "chrome/browser/extensions/api/bookmarks/bookmark_api_helpers.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_uma.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/notification_types.h"
#include "ui/base/l10n/l10n_util.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::ManagedBookmarkService;

namespace extensions {

namespace keys = bookmark_api_constants;
namespace bookmarks = api::bookmarks;

using bookmarks::BookmarkTreeNode;
using bookmarks::CreateDetails;
using content::BrowserContext;
using content::BrowserThread;
using content::WebContents;

namespace {

// Generates a default path (including a default filename) that will be
// used for pre-populating the "Export Bookmarks" file chooser dialog box.
base::FilePath GetDefaultFilepathForBookmarkExport() {
  base::Time time = base::Time::Now();

  // Concatenate a date stamp to the filename.
#if defined(OS_POSIX)
  base::FilePath::StringType filename =
      l10n_util::GetStringFUTF8(IDS_EXPORT_BOOKMARKS_DEFAULT_FILENAME,
                                base::TimeFormatShortDateNumeric(time));
#elif defined(OS_WIN)
  base::FilePath::StringType filename =
      l10n_util::GetStringFUTF16(IDS_EXPORT_BOOKMARKS_DEFAULT_FILENAME,
                                 base::TimeFormatShortDateNumeric(time));
#endif

  base::i18n::ReplaceIllegalCharactersInPath(&filename, '_');

  base::FilePath default_path;
  PathService::Get(chrome::DIR_USER_DOCUMENTS, &default_path);
  return default_path.Append(filename);
}

}  // namespace

bool BookmarksFunction::RunAsync() {
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  if (!model->loaded()) {
    // Bookmarks are not ready yet.  We'll wait.
    model->AddObserver(this);
    AddRef();  // Balanced in Loaded().
    return true;
  }

  RunAndSendResponse();
  return true;
}

BookmarkModel* BookmarksFunction::GetBookmarkModel() {
  return BookmarkModelFactory::GetForBrowserContext(GetProfile());
}

ManagedBookmarkService* BookmarksFunction::GetManagedBookmarkService() {
  return ManagedBookmarkServiceFactory::GetForProfile(GetProfile());
}

bool BookmarksFunction::GetBookmarkIdAsInt64(const std::string& id_string,
                                             int64_t* id) {
  if (base::StringToInt64(id_string, id))
    return true;

  error_ = keys::kInvalidIdError;
  return false;
}

const BookmarkNode* BookmarksFunction::GetBookmarkNodeFromId(
    const std::string& id_string) {
  int64_t id;
  if (!GetBookmarkIdAsInt64(id_string, &id))
    return NULL;

  const BookmarkNode* node = ::bookmarks::GetBookmarkNodeByID(
      BookmarkModelFactory::GetForBrowserContext(GetProfile()), id);
  if (!node)
    error_ = keys::kNoNodeError;

  return node;
}

const BookmarkNode* BookmarksFunction::CreateBookmarkNode(
    BookmarkModel* model,
    const CreateDetails& details,
    const BookmarkNode::MetaInfoMap* meta_info) {
  int64_t parentId;

  if (!details.parent_id.get()) {
    // Optional, default to "other bookmarks".
    parentId = model->other_node()->id();
  } else {
    if (!GetBookmarkIdAsInt64(*details.parent_id, &parentId))
      return NULL;
  }
  const BookmarkNode* parent =
      ::bookmarks::GetBookmarkNodeByID(model, parentId);
  if (!CanBeModified(parent))
    return NULL;

  int index;
  if (!details.index.get()) {  // Optional (defaults to end).
    index = parent->child_count();
  } else {
    index = *details.index;
    if (index > parent->child_count() || index < 0) {
      error_ = keys::kInvalidIndexError;
      return NULL;
    }
  }

  base::string16 title;  // Optional.
  if (details.title.get())
    title = base::UTF8ToUTF16(*details.title);

  std::string url_string;  // Optional.
  if (details.url.get())
    url_string = *details.url;

  GURL url(url_string);
  if (!url_string.empty() && !url.is_valid()) {
    error_ = keys::kInvalidUrlError;
    return NULL;
  }

  const BookmarkNode* node;
  if (url_string.length()) {
    node = model->AddURLWithCreationTimeAndMetaInfo(
        parent, index, title, url, base::Time::Now(), meta_info);
  } else {
    node = model->AddFolderWithMetaInfo(parent, index, title, meta_info);
    model->SetDateFolderModified(parent, base::Time::Now());
  }

  DCHECK(node);

  return node;
}

bool BookmarksFunction::EditBookmarksEnabled() {
  PrefService* prefs = user_prefs::UserPrefs::Get(GetProfile());
  if (prefs->GetBoolean(::bookmarks::prefs::kEditBookmarksEnabled))
    return true;
  error_ = keys::kEditBookmarksDisabled;
  return false;
}

bool BookmarksFunction::CanBeModified(const BookmarkNode* node) {
  if (!node) {
    error_ = keys::kNoParentError;
    return false;
  }
  if (node->is_root()) {
    error_ = keys::kModifySpecialError;
    return false;
  }
  ManagedBookmarkService* managed = GetManagedBookmarkService();
  if (::bookmarks::IsDescendantOf(node, managed->managed_node())) {
    error_ = keys::kModifyManagedError;
    return false;
  }
  return true;
}

void BookmarksFunction::BookmarkModelChanged() {
}

void BookmarksFunction::BookmarkModelLoaded(BookmarkModel* model,
                                            bool ids_reassigned) {
  model->RemoveObserver(this);
  RunAndSendResponse();
  Release();  // Balanced in RunOnReady().
}

void BookmarksFunction::RunAndSendResponse() {
  bool success = RunOnReady();
  if (success) {
    content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_BOOKMARKS_API_INVOKED,
      content::Source<const Extension>(extension()),
      content::Details<const BookmarksFunction>(this));
  }
  SendResponse(success);
}

BookmarkEventRouter::BookmarkEventRouter(Profile* profile)
    : browser_context_(profile),
      model_(BookmarkModelFactory::GetForBrowserContext(profile)),
      managed_(ManagedBookmarkServiceFactory::GetForProfile(profile)) {
  model_->AddObserver(this);
}

BookmarkEventRouter::~BookmarkEventRouter() {
  if (model_) {
    model_->RemoveObserver(this);
  }
}

void BookmarkEventRouter::DispatchEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->BroadcastEvent(std::make_unique<extensions::Event>(
        histogram_value, event_name, std::move(event_args)));
  }
}

void BookmarkEventRouter::BookmarkModelLoaded(BookmarkModel* model,
                                              bool ids_reassigned) {
  // TODO(erikkay): Perhaps we should send this event down to the extension
  // so they know when it's safe to use the API?
}

void BookmarkEventRouter::BookmarkModelBeingDeleted(BookmarkModel* model) {
  model_ = NULL;
}

void BookmarkEventRouter::BookmarkNodeMoved(BookmarkModel* model,
                                            const BookmarkNode* old_parent,
                                            int old_index,
                                            const BookmarkNode* new_parent,
                                            int new_index) {
  const BookmarkNode* node = new_parent->GetChild(new_index);
  bookmarks::OnMoved::MoveInfo move_info;
  move_info.parent_id = base::Int64ToString(new_parent->id());
  move_info.index = new_index;
  move_info.old_parent_id = base::Int64ToString(old_parent->id());
  move_info.old_index = old_index;

  DispatchEvent(
      events::BOOKMARKS_ON_MOVED, bookmarks::OnMoved::kEventName,
      bookmarks::OnMoved::Create(base::Int64ToString(node->id()), move_info));
}

void BookmarkEventRouter::BookmarkNodeAdded(BookmarkModel* model,
                                            const BookmarkNode* parent,
                                            int index) {
  const BookmarkNode* node = parent->GetChild(index);
  BookmarkTreeNode tree_node =
      bookmark_api_helpers::GetBookmarkTreeNode(managed_, node, false, false);
  DispatchEvent(
      events::BOOKMARKS_ON_CREATED, bookmarks::OnCreated::kEventName,
      bookmarks::OnCreated::Create(base::Int64ToString(node->id()), tree_node));
}

void BookmarkEventRouter::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    int index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  bookmarks::OnRemoved::RemoveInfo remove_info;
  remove_info.parent_id = base::Int64ToString(parent->id());
  remove_info.index = index;
  bookmark_api_helpers::PopulateBookmarkTreeNode(managed_, node, true, false,
                                                 &remove_info.node);

  DispatchEvent(events::BOOKMARKS_ON_REMOVED, bookmarks::OnRemoved::kEventName,
                bookmarks::OnRemoved::Create(base::Int64ToString(node->id()),
                                             remove_info));
}

void BookmarkEventRouter::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  NOTREACHED();
  // TODO(shashishekhar) Currently this notification is only used on Android,
  // which does not support extensions. If Desktop needs to support this, add
  // a new event to the extensions api.
}

void BookmarkEventRouter::BookmarkNodeChanged(BookmarkModel* model,
                                              const BookmarkNode* node) {
  // TODO(erikkay) The only three things that BookmarkModel sends this
  // notification for are title, url and favicon.  Since we're currently
  // ignoring favicon and since the notification doesn't say which one anyway,
  // for now we only include title and url.  The ideal thing would be to change
  // BookmarkModel to indicate what changed.
  bookmarks::OnChanged::ChangeInfo change_info;
  change_info.title = base::UTF16ToUTF8(node->GetTitle());
  if (node->is_url())
    change_info.url.reset(new std::string(node->url().spec()));

  DispatchEvent(events::BOOKMARKS_ON_CHANGED, bookmarks::OnChanged::kEventName,
                bookmarks::OnChanged::Create(base::Int64ToString(node->id()),
                                             change_info));
}

void BookmarkEventRouter::BookmarkNodeFaviconChanged(BookmarkModel* model,
                                                     const BookmarkNode* node) {
  // TODO(erikkay) anything we should do here?
}

void BookmarkEventRouter::BookmarkNodeChildrenReordered(
    BookmarkModel* model,
    const BookmarkNode* node) {
  bookmarks::OnChildrenReordered::ReorderInfo reorder_info;
  int childCount = node->child_count();
  for (int i = 0; i < childCount; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    reorder_info.child_ids.push_back(base::Int64ToString(child->id()));
  }

  DispatchEvent(events::BOOKMARKS_ON_CHILDREN_REORDERED,
                bookmarks::OnChildrenReordered::kEventName,
                bookmarks::OnChildrenReordered::Create(
                    base::Int64ToString(node->id()), reorder_info));
}

void BookmarkEventRouter::ExtensiveBookmarkChangesBeginning(
    BookmarkModel* model) {
  DispatchEvent(events::BOOKMARKS_ON_IMPORT_BEGAN,
                bookmarks::OnImportBegan::kEventName,
                bookmarks::OnImportBegan::Create());
}

void BookmarkEventRouter::ExtensiveBookmarkChangesEnded(BookmarkModel* model) {
  DispatchEvent(events::BOOKMARKS_ON_IMPORT_ENDED,
                bookmarks::OnImportEnded::kEventName,
                bookmarks::OnImportEnded::Create());
}

BookmarksAPI::BookmarksAPI(BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, bookmarks::OnCreated::kEventName);
  event_router->RegisterObserver(this, bookmarks::OnRemoved::kEventName);
  event_router->RegisterObserver(this, bookmarks::OnChanged::kEventName);
  event_router->RegisterObserver(this, bookmarks::OnMoved::kEventName);
  event_router->RegisterObserver(this,
                                 bookmarks::OnChildrenReordered::kEventName);
  event_router->RegisterObserver(this, bookmarks::OnImportBegan::kEventName);
  event_router->RegisterObserver(this, bookmarks::OnImportEnded::kEventName);
}

BookmarksAPI::~BookmarksAPI() {
}

void BookmarksAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<BookmarksAPI>>::
    DestructorAtExit g_bookmarks_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<BookmarksAPI>*
BookmarksAPI::GetFactoryInstance() {
  return g_bookmarks_api_factory.Pointer();
}

void BookmarksAPI::OnListenerAdded(const EventListenerInfo& details) {
  bookmark_event_router_.reset(
      new BookmarkEventRouter(Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

bool BookmarksGetFunction::RunOnReady() {
  std::unique_ptr<bookmarks::Get::Params> params(
      bookmarks::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<BookmarkTreeNode> nodes;
  ManagedBookmarkService* managed = GetManagedBookmarkService();
  if (params->id_or_id_list.as_strings) {
    std::vector<std::string>& ids = *params->id_or_id_list.as_strings;
    size_t count = ids.size();
    EXTENSION_FUNCTION_VALIDATE(count > 0);
    for (size_t i = 0; i < count; ++i) {
      const BookmarkNode* node = GetBookmarkNodeFromId(ids[i]);
      if (!node)
        return false;
      bookmark_api_helpers::AddNode(managed, node, &nodes, false);
    }
  } else {
    const BookmarkNode* node =
        GetBookmarkNodeFromId(*params->id_or_id_list.as_string);
    if (!node)
      return false;
    bookmark_api_helpers::AddNode(managed, node, &nodes, false);
  }

  results_ = bookmarks::Get::Results::Create(nodes);
  return true;
}

bool BookmarksGetChildrenFunction::RunOnReady() {
  std::unique_ptr<bookmarks::GetChildren::Params> params(
      bookmarks::GetChildren::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const BookmarkNode* node = GetBookmarkNodeFromId(params->id);
  if (!node)
    return false;

  std::vector<BookmarkTreeNode> nodes;
  int child_count = node->child_count();
  for (int i = 0; i < child_count; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    bookmark_api_helpers::AddNode(GetManagedBookmarkService(), child, &nodes,
                                  false);
  }

  results_ = bookmarks::GetChildren::Results::Create(nodes);
  return true;
}

bool BookmarksGetRecentFunction::RunOnReady() {
  std::unique_ptr<bookmarks::GetRecent::Params> params(
      bookmarks::GetRecent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->number_of_items < 1)
    return false;

  std::vector<const BookmarkNode*> nodes;
  ::bookmarks::GetMostRecentlyAddedEntries(
      BookmarkModelFactory::GetForBrowserContext(GetProfile()),
      params->number_of_items, &nodes);

  std::vector<BookmarkTreeNode> tree_nodes;
  for (const BookmarkNode* node : nodes) {
    bookmark_api_helpers::AddNode(GetManagedBookmarkService(), node,
                                  &tree_nodes, false);
  }

  results_ = bookmarks::GetRecent::Results::Create(tree_nodes);
  return true;
}

bool BookmarksGetTreeFunction::RunOnReady() {
  std::vector<BookmarkTreeNode> nodes;
  const BookmarkNode* node =
      BookmarkModelFactory::GetForBrowserContext(GetProfile())->root_node();
  bookmark_api_helpers::AddNode(GetManagedBookmarkService(), node, &nodes,
                                true);
  results_ = bookmarks::GetTree::Results::Create(nodes);
  return true;
}

bool BookmarksGetSubTreeFunction::RunOnReady() {
  std::unique_ptr<bookmarks::GetSubTree::Params> params(
      bookmarks::GetSubTree::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const BookmarkNode* node = GetBookmarkNodeFromId(params->id);
  if (!node)
    return false;

  std::vector<BookmarkTreeNode> nodes;
  bookmark_api_helpers::AddNode(GetManagedBookmarkService(), node, &nodes,
                                true);
  results_ = bookmarks::GetSubTree::Results::Create(nodes);
  return true;
}

bool BookmarksSearchFunction::RunOnReady() {
  std::unique_ptr<bookmarks::Search::Params> params(
      bookmarks::Search::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<const BookmarkNode*> nodes;
  if (params->query.as_string) {
    ::bookmarks::QueryFields query;
    query.word_phrase_query.reset(
        new base::string16(base::UTF8ToUTF16(*params->query.as_string)));
    ::bookmarks::GetBookmarksMatchingProperties(
        BookmarkModelFactory::GetForBrowserContext(GetProfile()), query,
        std::numeric_limits<int>::max(), &nodes);
  } else {
    DCHECK(params->query.as_object);
    const bookmarks::Search::Params::Query::Object& object =
        *params->query.as_object;
    ::bookmarks::QueryFields query;
    if (object.query) {
      query.word_phrase_query.reset(
          new base::string16(base::UTF8ToUTF16(*object.query)));
    }
    if (object.url)
      query.url.reset(new base::string16(base::UTF8ToUTF16(*object.url)));
    if (object.title)
      query.title.reset(new base::string16(base::UTF8ToUTF16(*object.title)));
    ::bookmarks::GetBookmarksMatchingProperties(
        BookmarkModelFactory::GetForBrowserContext(GetProfile()), query,
        std::numeric_limits<int>::max(), &nodes);
  }

  std::vector<BookmarkTreeNode> tree_nodes;
  ManagedBookmarkService* managed = GetManagedBookmarkService();
  for (const BookmarkNode* node : nodes)
    bookmark_api_helpers::AddNode(managed, node, &tree_nodes, false);

  results_ = bookmarks::Search::Results::Create(tree_nodes);
  return true;
}

bool BookmarksRemoveFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;

  std::unique_ptr<bookmarks::Remove::Params> params(
      bookmarks::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int64_t id;
  if (!GetBookmarkIdAsInt64(params->id, &id))
    return false;

  bool recursive = false;
  if (name() == BookmarksRemoveTreeFunction::function_name())
    recursive = true;

  BookmarkModel* model = GetBookmarkModel();
  ManagedBookmarkService* managed = GetManagedBookmarkService();
  if (!bookmark_api_helpers::RemoveNode(model, managed, id, recursive, &error_))
    return false;

  return true;
}

bool BookmarksCreateFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;

  std::unique_ptr<bookmarks::Create::Params> params(
      bookmarks::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  const BookmarkNode* node = CreateBookmarkNode(model, params->bookmark, NULL);
  if (!node)
    return false;

  BookmarkTreeNode ret = bookmark_api_helpers::GetBookmarkTreeNode(
      GetManagedBookmarkService(), node, false, false);
  results_ = bookmarks::Create::Results::Create(ret);

  return true;
}

bool BookmarksMoveFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;

  std::unique_ptr<bookmarks::Move::Params> params(
      bookmarks::Move::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const BookmarkNode* node = GetBookmarkNodeFromId(params->id);
  if (!node)
    return false;

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  if (model->is_permanent_node(node)) {
    error_ = keys::kModifySpecialError;
    return false;
  }

  const BookmarkNode* parent = NULL;
  if (!params->destination.parent_id.get()) {
    // Optional, defaults to current parent.
    parent = node->parent();
  } else {
    int64_t parentId;
    if (!GetBookmarkIdAsInt64(*params->destination.parent_id, &parentId))
      return false;

    parent = ::bookmarks::GetBookmarkNodeByID(model, parentId);
  }
  if (!CanBeModified(parent) || !CanBeModified(node))
    return false;

  int index;
  if (params->destination.index.get()) {  // Optional (defaults to end).
    index = *params->destination.index;
    if (index > parent->child_count() || index < 0) {
      error_ = keys::kInvalidIndexError;
      return false;
    }
  } else {
    index = parent->child_count();
  }

  model->Move(node, parent, index);

  BookmarkTreeNode tree_node = bookmark_api_helpers::GetBookmarkTreeNode(
      GetManagedBookmarkService(), node, false, false);
  results_ = bookmarks::Move::Results::Create(tree_node);

  return true;
}

bool BookmarksUpdateFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;

  std::unique_ptr<bookmarks::Update::Params> params(
      bookmarks::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Optional but we need to distinguish non present from an empty title.
  base::string16 title;
  bool has_title = false;
  if (params->changes.title.get()) {
    title = base::UTF8ToUTF16(*params->changes.title);
    has_title = true;
  }

  // Optional.
  std::string url_string;
  if (params->changes.url.get())
    url_string = *params->changes.url;
  GURL url(url_string);
  if (!url_string.empty() && !url.is_valid()) {
    error_ = keys::kInvalidUrlError;
    return false;
  }

  const BookmarkNode* node = GetBookmarkNodeFromId(params->id);
  if (!CanBeModified(node))
    return false;

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  if (model->is_permanent_node(node)) {
    error_ = keys::kModifySpecialError;
    return false;
  }
  if (has_title)
    model->SetTitle(node, title);
  if (!url.is_empty())
    model->SetURL(node, url);

  BookmarkTreeNode tree_node = bookmark_api_helpers::GetBookmarkTreeNode(
      GetManagedBookmarkService(), node, false, false);
  results_ = bookmarks::Update::Results::Create(tree_node);
  return true;
}

BookmarksIOFunction::BookmarksIOFunction() {}

BookmarksIOFunction::~BookmarksIOFunction() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void BookmarksIOFunction::ShowSelectFileDialog(
    ui::SelectFileDialog::Type type,
    const base::FilePath& default_path) {
  if (!dispatcher())
    return;  // Extension was unloaded.

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Balanced in one of the three callbacks of SelectFileDialog:
  // either FileSelectionCanceled, MultiFilesSelected, or FileSelected
  AddRef();

  WebContents* web_contents = GetAssociatedWebContents();

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("html"));
  gfx::NativeWindow owning_window = web_contents ?
      platform_util::GetTopLevel(web_contents->GetNativeView())
          : NULL;
  // |web_contents| can be NULL (for background pages), which is fine. In such
  // a case if file-selection dialogs are forbidden by policy, we will not
  // show an InfoBar, which is better than letting one appear out of the blue.
  select_file_dialog_->SelectFile(type,
                                  base::string16(),
                                  default_path,
                                  &file_type_info,
                                  0,
                                  base::FilePath::StringType(),
                                  owning_window,
                                  NULL);
}

void BookmarksIOFunction::FileSelectionCanceled(void* params) {
  Release();  // Balanced in BookmarksIOFunction::SelectFile()
}

void BookmarksIOFunction::MultiFilesSelected(
    const std::vector<base::FilePath>& files, void* params) {
  Release();  // Balanced in BookmarsIOFunction::SelectFile()
  NOTREACHED() << "Should not be able to select multiple files";
}

bool BookmarksImportFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;
  ShowSelectFileDialog(ui::SelectFileDialog::SELECT_OPEN_FILE,
                       base::FilePath());
  return true;
}

void BookmarksImportFunction::FileSelected(const base::FilePath& path,
                                           int index,
                                           void* params) {
  // Deletes itself.
  ExternalProcessImporterHost* importer_host = new ExternalProcessImporterHost;
  importer::SourceProfile source_profile;
  source_profile.importer_type = importer::TYPE_BOOKMARKS_FILE;
  source_profile.source_path = path;
  importer_host->StartImportSettings(source_profile,
                                     GetProfile(),
                                     importer::FAVORITES,
                                     new ProfileWriter(GetProfile()));

  importer::LogImporterUseToMetrics("BookmarksAPI",
                                    importer::TYPE_BOOKMARKS_FILE);
  Release();  // Balanced in BookmarksIOFunction::SelectFile()
}

bool BookmarksExportFunction::RunOnReady() {
  // "bookmarks.export" is exposed to a small number of extensions. These
  // extensions use user gesture for export, so use USER_VISIBLE priority.
  // GetDefaultFilepathForBookmarkExport() might have to touch filesystem
  // (stat or access, for example), so this requires IO.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetDefaultFilepathForBookmarkExport),
      base::BindOnce(&BookmarksIOFunction::ShowSelectFileDialog, this,
                     ui::SelectFileDialog::SELECT_SAVEAS_FILE));
  return true;
}

void BookmarksExportFunction::FileSelected(const base::FilePath& path,
                                           int index,
                                           void* params) {
  bookmark_html_writer::WriteBookmarks(GetProfile(), path, NULL);
  Release();  // Balanced in BookmarksIOFunction::SelectFile()
}

}  // namespace extensions
