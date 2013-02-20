// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/sha1.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_codec.h"
#include "chrome/browser/bookmarks/bookmark_html_writer.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/extensions/api/bookmarks/bookmark_api_constants.h"
#include "chrome/browser/extensions/api/bookmarks/bookmark_api_helpers.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace keys = bookmark_api_constants;
namespace bookmarks = api::bookmarks;

using base::TimeDelta;
using bookmarks::BookmarkTreeNode;
using content::BrowserThread;
using content::WebContents;

typedef QuotaLimitHeuristic::Bucket Bucket;
typedef QuotaLimitHeuristic::Config Config;
typedef QuotaLimitHeuristic::BucketList BucketList;
typedef ExtensionsQuotaService::TimedLimit TimedLimit;
typedef ExtensionsQuotaService::SustainedLimit SustainedLimit;
typedef QuotaLimitHeuristic::BucketMapper BucketMapper;

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

  file_util::ReplaceIllegalCharactersInPath(&filename, '_');

  base::FilePath default_path;
  PathService::Get(chrome::DIR_USER_DOCUMENTS, &default_path);
  return default_path.Append(filename);
}

}  // namespace

void BookmarksFunction::Run() {
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  if (!model->IsLoaded()) {
    // Bookmarks are not ready yet.  We'll wait.
    model->AddObserver(this);
    AddRef();  // Balanced in Loaded().
    return;
  }

  bool success = RunImpl();
  if (success) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_BOOKMARKS_API_INVOKED,
        content::Source<const Extension>(GetExtension()),
        content::Details<const BookmarksFunction>(this));
  }
  SendResponse(success);
}

bool BookmarksFunction::GetBookmarkIdAsInt64(const std::string& id_string,
                                             int64* id) {
  if (base::StringToInt64(id_string, id))
    return true;

  error_ = keys::kInvalidIdError;
  return false;
}

bool BookmarksFunction::EditBookmarksEnabled() {
  PrefService* prefs = PrefServiceFromBrowserContext(profile_);
  if (prefs->GetBoolean(prefs::kEditBookmarksEnabled))
    return true;
  error_ = keys::kEditBookmarksDisabled;
  return false;
}

void BookmarksFunction::BookmarkModelChanged() {
}

void BookmarksFunction::Loaded(BookmarkModel* model, bool ids_reassigned) {
  model->RemoveObserver(this);
  Run();
  Release();  // Balanced in Run().
}

BookmarkEventRouter::BookmarkEventRouter(BookmarkModel* model) : model_(model) {
  model_->AddObserver(this);
}

BookmarkEventRouter::~BookmarkEventRouter() {
  if (model_) {
    model_->RemoveObserver(this);
  }
}

void BookmarkEventRouter::DispatchEvent(
    Profile* profile,
    const char* event_name,
    scoped_ptr<ListValue> event_args) {
  if (extensions::ExtensionSystem::Get(profile)->event_router()) {
    extensions::ExtensionSystem::Get(profile)->event_router()->BroadcastEvent(
        make_scoped_ptr(new extensions::Event(event_name, event_args.Pass())));
  }
}

void BookmarkEventRouter::Loaded(BookmarkModel* model, bool ids_reassigned) {
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
  scoped_ptr<ListValue> args(new ListValue());
  const BookmarkNode* node = new_parent->GetChild(new_index);
  args->Append(new StringValue(base::Int64ToString(node->id())));
  DictionaryValue* object_args = new DictionaryValue();
  object_args->SetString(keys::kParentIdKey,
                         base::Int64ToString(new_parent->id()));
  object_args->SetInteger(keys::kIndexKey, new_index);
  object_args->SetString(keys::kOldParentIdKey,
                         base::Int64ToString(old_parent->id()));
  object_args->SetInteger(keys::kOldIndexKey, old_index);
  args->Append(object_args);

  DispatchEvent(model->profile(), keys::kOnBookmarkMoved, args.Pass());
}

void BookmarkEventRouter::BookmarkNodeAdded(BookmarkModel* model,
                                            const BookmarkNode* parent,
                                            int index) {
  scoped_ptr<ListValue> args(new ListValue());
  const BookmarkNode* node = parent->GetChild(index);
  args->Append(new StringValue(base::Int64ToString(node->id())));
  scoped_ptr<BookmarkTreeNode> tree_node(
      bookmark_api_helpers::GetBookmarkTreeNode(node, false, false));
  args->Append(tree_node->ToValue().release());

  DispatchEvent(model->profile(), keys::kOnBookmarkCreated, args.Pass());
}

void BookmarkEventRouter::BookmarkNodeRemoved(BookmarkModel* model,
                                              const BookmarkNode* parent,
                                              int index,
                                              const BookmarkNode* node) {
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(new StringValue(base::Int64ToString(node->id())));
  DictionaryValue* object_args = new DictionaryValue();
  object_args->SetString(keys::kParentIdKey,
                         base::Int64ToString(parent->id()));
  object_args->SetInteger(keys::kIndexKey, index);
  args->Append(object_args);

  DispatchEvent(model->profile(), keys::kOnBookmarkRemoved, args.Pass());
}

void BookmarkEventRouter::BookmarkNodeChanged(BookmarkModel* model,
                                              const BookmarkNode* node) {
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(new StringValue(base::Int64ToString(node->id())));

  // TODO(erikkay) The only three things that BookmarkModel sends this
  // notification for are title, url and favicon.  Since we're currently
  // ignoring favicon and since the notification doesn't say which one anyway,
  // for now we only include title and url.  The ideal thing would be to change
  // BookmarkModel to indicate what changed.
  DictionaryValue* object_args = new DictionaryValue();
  object_args->SetString(keys::kTitleKey, node->GetTitle());
  if (node->is_url())
    object_args->SetString(keys::kUrlKey, node->url().spec());
  args->Append(object_args);

  DispatchEvent(model->profile(), keys::kOnBookmarkChanged, args.Pass());
}

void BookmarkEventRouter::BookmarkNodeFaviconChanged(BookmarkModel* model,
                                                     const BookmarkNode* node) {
  // TODO(erikkay) anything we should do here?
}

void BookmarkEventRouter::BookmarkNodeChildrenReordered(
    BookmarkModel* model,
    const BookmarkNode* node) {
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(new StringValue(base::Int64ToString(node->id())));
  int childCount = node->child_count();
  ListValue* children = new ListValue();
  for (int i = 0; i < childCount; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    Value* child_id = new StringValue(base::Int64ToString(child->id()));
    children->Append(child_id);
  }
  DictionaryValue* reorder_info = new DictionaryValue();
  reorder_info->Set(keys::kChildIdsKey, children);
  args->Append(reorder_info);

  DispatchEvent(model->profile(), keys::kOnBookmarkChildrenReordered,
                args.Pass());
}

void BookmarkEventRouter::ExtensiveBookmarkChangesBeginning(
    BookmarkModel* model) {
  scoped_ptr<ListValue> args(new ListValue());
  DispatchEvent(model->profile(),
                keys::kOnBookmarkImportBegan,
                args.Pass());
}

void BookmarkEventRouter::ExtensiveBookmarkChangesEnded(BookmarkModel* model) {
  scoped_ptr<ListValue> args(new ListValue());
  DispatchEvent(model->profile(),
                keys::kOnBookmarkImportEnded,
                args.Pass());
}

BookmarksAPI::BookmarksAPI(Profile* profile) : profile_(profile) {
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, keys::kOnBookmarkCreated);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, keys::kOnBookmarkRemoved);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, keys::kOnBookmarkChanged);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, keys::kOnBookmarkMoved);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, keys::kOnBookmarkChildrenReordered);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, keys::kOnBookmarkImportBegan);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, keys::kOnBookmarkImportEnded);
}

BookmarksAPI::~BookmarksAPI() {
}

void BookmarksAPI::Shutdown() {
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

static base::LazyInstance<ProfileKeyedAPIFactory<BookmarksAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<BookmarksAPI>* BookmarksAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

void BookmarksAPI::OnListenerAdded(const EventListenerInfo& details) {
  bookmark_event_router_.reset(new BookmarkEventRouter(
      BookmarkModelFactory::GetForProfile(profile_)));
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

bool BookmarksGetTreeFunction::RunImpl() {
  scoped_ptr<bookmarks::Get::Params> params(
      bookmarks::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<linked_ptr<BookmarkTreeNode> > nodes;
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  if (params->id_or_id_list.as_array) {
    std::vector<std::string>* ids = params->id_or_id_list.as_array.get();
    size_t count = ids->size();
    EXTENSION_FUNCTION_VALIDATE(count > 0);
    for (size_t i = 0; i < count; ++i) {
      int64 id;
      if (!GetBookmarkIdAsInt64(ids->at(i), &id))
        return false;
      const BookmarkNode* node = model->GetNodeByID(id);
      if (!node) {
        error_ = keys::kNoNodeError;
        return false;
      } else {
        bookmark_api_helpers::AddNode(node, &nodes, false);
      }
    }
  } else {
    int64 id;
    if (!GetBookmarkIdAsInt64(*params->id_or_id_list.as_string, &id))
      return false;
    const BookmarkNode* node = model->GetNodeByID(id);
    if (!node) {
      error_ = keys::kNoNodeError;
      return false;
    }
    bookmark_api_helpers::AddNode(node, &nodes, false);
  }

  results_ = bookmarks::Get::Results::Create(nodes);
  return true;
}

bool BookmarksGetChildrenFunction::RunImpl() {
  scoped_ptr<bookmarks::GetChildren::Params> params(
      bookmarks::GetChildren::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int64 id;
  if (!GetBookmarkIdAsInt64(params->id, &id))
    return false;

  std::vector<linked_ptr<BookmarkTreeNode> > nodes;
  const BookmarkNode* node =
      BookmarkModelFactory::GetForProfile(profile())->GetNodeByID(id);
  if (!node) {
    error_ = keys::kNoNodeError;
    return false;
  }
  int child_count = node->child_count();
  for (int i = 0; i < child_count; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    bookmark_api_helpers::AddNode(child, &nodes, false);
  }

  results_ = bookmarks::GetChildren::Results::Create(nodes);
  return true;
}

bool BookmarksGetFunction::RunImpl() {
  scoped_ptr<bookmarks::GetRecent::Params> params(
      bookmarks::GetRecent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->number_of_items < 1)
    return false;

  std::vector<const BookmarkNode*> nodes;
  bookmark_utils::GetMostRecentlyAddedEntries(
      BookmarkModelFactory::GetForProfile(profile()),
      params->number_of_items,
      &nodes);

  std::vector<linked_ptr<BookmarkTreeNode> > tree_nodes;
  std::vector<const BookmarkNode*>::iterator i = nodes.begin();
  for (; i != nodes.end(); ++i) {
    const BookmarkNode* node = *i;
    bookmark_api_helpers::AddNode(node, &tree_nodes, false);
  }

  results_ = bookmarks::GetRecent::Results::Create(tree_nodes);
  return true;
}

bool BookmarksGetSubTreeFunction::RunImpl() {
  std::vector<linked_ptr<BookmarkTreeNode> > nodes;
  const BookmarkNode* node =
      BookmarkModelFactory::GetForProfile(profile())->root_node();
  bookmark_api_helpers::AddNode(node, &nodes, true);
  results_ = bookmarks::GetTree::Results::Create(nodes);
  return true;
}

bool BookmarksGetRecentFunction::RunImpl() {
  scoped_ptr<bookmarks::GetSubTree::Params> params(
      bookmarks::GetSubTree::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int64 id;
  if (!GetBookmarkIdAsInt64(params->id, &id))
    return false;

  const BookmarkNode* node =
      BookmarkModelFactory::GetForProfile(profile())->GetNodeByID(id);
  if (!node) {
    error_ = keys::kNoNodeError;
    return false;
  }

  std::vector<linked_ptr<BookmarkTreeNode> > nodes;
  bookmark_api_helpers::AddNode(node, &nodes, true);
  results_ = bookmarks::GetSubTree::Results::Create(nodes);
  return true;
}

bool BookmarksSearchFunction::RunImpl() {
  scoped_ptr<bookmarks::Search::Params> params(
      bookmarks::Search::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  PrefService* prefs = PrefServiceFromBrowserContext(profile_);
  std::string lang = prefs->GetString(prefs::kAcceptLanguages);
  std::vector<const BookmarkNode*> nodes;
  bookmark_utils::GetBookmarksContainingText(
      BookmarkModelFactory::GetForProfile(profile()),
      UTF8ToUTF16(params->query),
      std::numeric_limits<int>::max(),
      lang,
      &nodes);

  std::vector<linked_ptr<BookmarkTreeNode> > tree_nodes;
  for (std::vector<const BookmarkNode*>::iterator node_iter = nodes.begin();
       node_iter != nodes.end(); ++node_iter) {
    bookmark_api_helpers::AddNode(*node_iter, &tree_nodes, false);
  }

  results_ = bookmarks::Search::Results::Create(tree_nodes);
  return true;
}

// static
bool BookmarksRemoveFunction::ExtractIds(const ListValue* args,
                                        std::list<int64>* ids,
                                        bool* invalid_id) {
  std::string id_string;
  if (!args->GetString(0, &id_string))
    return false;
  int64 id;
  if (base::StringToInt64(id_string, &id))
    ids->push_back(id);
  else
    *invalid_id = true;
  return true;
}

bool BookmarksRemoveFunction::RunImpl() {
  if (!EditBookmarksEnabled())
    return false;

  scoped_ptr<bookmarks::Remove::Params> params(
      bookmarks::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int64 id;
  if (!base::StringToInt64(params->id, &id)) {
    error_ = keys::kInvalidIdError;
    return false;
  }

  bool recursive = false;
  if (name() == BookmarksRemoveTreeFunction::function_name())
    recursive = true;

  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  if (!bookmark_api_helpers::RemoveNode(model, id, recursive, &error_))
    return false;

  return true;
}

bool BookmarksCreateFunction::RunImpl() {
  if (!EditBookmarksEnabled())
    return false;

  scoped_ptr<bookmarks::Create::Params> params(
      bookmarks::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  int64 parentId;

  if (!params->bookmark.parent_id.get()) {
    // Optional, default to "other bookmarks".
    parentId = model->other_node()->id();
  } else {
    if (!GetBookmarkIdAsInt64(*params->bookmark.parent_id, &parentId))
      return false;
  }
  const BookmarkNode* parent = model->GetNodeByID(parentId);
  if (!parent) {
    error_ = keys::kNoParentError;
    return false;
  }
  if (parent->is_root()) {  // Can't create children of the root.
    error_ = keys::kModifySpecialError;
    return false;
  }

  int index;
  if (!params->bookmark.index.get()) {  // Optional (defaults to end).
    index = parent->child_count();
  } else {
    index = *params->bookmark.index;
    if (index > parent->child_count() || index < 0) {
      error_ = keys::kInvalidIndexError;
      return false;
    }
  }

  string16 title;  // Optional.
  if (params->bookmark.title.get())
    title = UTF8ToUTF16(*params->bookmark.title.get());

  std::string url_string;  // Optional.
  if (params->bookmark.url.get())
    url_string = *params->bookmark.url.get();

  GURL url(url_string);
  if (!url_string.empty() && !url.is_valid()) {
    error_ = keys::kInvalidUrlError;
    return false;
  }

  const BookmarkNode* node;
  if (url_string.length())
    node = model->AddURL(parent, index, title, url);
  else
    node = model->AddFolder(parent, index, title);
  DCHECK(node);
  if (!node) {
    error_ = keys::kNoNodeError;
    return false;
  }

  scoped_ptr<BookmarkTreeNode> ret(
      bookmark_api_helpers::GetBookmarkTreeNode(node, false, false));
  results_ = bookmarks::Create::Results::Create(*ret);

  return true;
}

// static
bool BookmarksMoveFunction::ExtractIds(const ListValue* args,
                                      std::list<int64>* ids,
                                      bool* invalid_id) {
  // For now, Move accepts ID parameters in the same way as an Update.
  return BookmarksUpdateFunction::ExtractIds(args, ids, invalid_id);
}

bool BookmarksMoveFunction::RunImpl() {
  if (!EditBookmarksEnabled())
    return false;

  scoped_ptr<bookmarks::Move::Params> params(
      bookmarks::Move::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int64 id;
  if (!base::StringToInt64(params->id, &id)) {
    error_ = keys::kInvalidIdError;
    return false;
  }

  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  const BookmarkNode* node = model->GetNodeByID(id);
  if (!node) {
    error_ = keys::kNoNodeError;
    return false;
  }
  if (model->is_permanent_node(node)) {
    error_ = keys::kModifySpecialError;
    return false;
  }

  const BookmarkNode* parent = NULL;
  if (!params->destination.parent_id.get()) {
    // Optional, defaults to current parent.
    parent = node->parent();
  } else {
    int64 parentId;
    if (!GetBookmarkIdAsInt64(*params->destination.parent_id, &parentId))
      return false;

    parent = model->GetNodeByID(parentId);
  }
  if (!parent) {
    error_ = keys::kNoParentError;
    // TODO(erikkay) return an error message.
    return false;
  }
  if (parent == model->root_node()) {
    error_ = keys::kModifySpecialError;
    return false;
  }

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

  scoped_ptr<BookmarkTreeNode> tree_node(
      bookmark_api_helpers::GetBookmarkTreeNode(node, false, false));
  results_ = bookmarks::Move::Results::Create(*tree_node);

  return true;
}

// static
bool BookmarksUpdateFunction::ExtractIds(const ListValue* args,
                                        std::list<int64>* ids,
                                        bool* invalid_id) {
  // For now, Update accepts ID parameters in the same way as an Remove.
  return BookmarksRemoveFunction::ExtractIds(args, ids, invalid_id);
}

bool BookmarksUpdateFunction::RunImpl() {
  if (!EditBookmarksEnabled())
    return false;

  scoped_ptr<bookmarks::Update::Params> params(
      bookmarks::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int64 id;
  if (!base::StringToInt64(params->id, &id)) {
    error_ = keys::kInvalidIdError;
    return false;
  }

  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());

  // Optional but we need to distinguish non present from an empty title.
  string16 title;
  bool has_title = false;
  if (params->changes.title.get()) {
    title = UTF8ToUTF16(*params->changes.title);
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

  const BookmarkNode* node = model->GetNodeByID(id);
  if (!node) {
    error_ = keys::kNoNodeError;
    return false;
  }
  if (model->is_permanent_node(node)) {
    error_ = keys::kModifySpecialError;
    return false;
  }
  if (has_title)
    model->SetTitle(node, title);
  if (!url.is_empty())
    model->SetURL(node, url);

  scoped_ptr<BookmarkTreeNode> tree_node(
      bookmark_api_helpers::GetBookmarkTreeNode(node, false, false));
  results_ = bookmarks::Update::Results::Create(*tree_node);
  return true;
}

// Mapper superclass for BookmarkFunctions.
template <typename BucketIdType>
class BookmarkBucketMapper : public BucketMapper {
 public:
  virtual ~BookmarkBucketMapper() { STLDeleteValues(&buckets_); }
 protected:
  Bucket* GetBucket(const BucketIdType& id) {
    Bucket* b = buckets_[id];
    if (b == NULL) {
      b = new Bucket();
      buckets_[id] = b;
    }
    return b;
  }
 private:
  std::map<BucketIdType, Bucket*> buckets_;
};

// Mapper for 'bookmarks.create'.  Maps "same input to bookmarks.create" to a
// unique bucket.
class CreateBookmarkBucketMapper : public BookmarkBucketMapper<std::string> {
 public:
  explicit CreateBookmarkBucketMapper(Profile* profile) : profile_(profile) {}
  // TODO(tim): This should share code with BookmarksCreateFunction::RunImpl,
  // but I can't figure out a good way to do that with all the macros.
  virtual void GetBucketsForArgs(const ListValue* args,
                                 BucketList* buckets) OVERRIDE {
    const DictionaryValue* json;
    if (!args->GetDictionary(0, &json))
      return;

    std::string parent_id;
    if (json->HasKey(keys::kParentIdKey)) {
      if (!json->GetString(keys::kParentIdKey, &parent_id))
        return;
    }
    BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile_);

    int64 parent_id_int64;
    base::StringToInt64(parent_id, &parent_id_int64);
    const BookmarkNode* parent = model->GetNodeByID(parent_id_int64);
    if (!parent)
      return;

    std::string bucket_id = UTF16ToUTF8(parent->GetTitle());
    std::string title;
    json->GetString(keys::kTitleKey, &title);
    std::string url_string;
    json->GetString(keys::kUrlKey, &url_string);

    bucket_id += title;
    bucket_id += url_string;
    // 20 bytes (SHA1 hash length) is very likely less than most of the
    // |bucket_id| strings we construct here, so we hash it to save space.
    buckets->push_back(GetBucket(base::SHA1HashString(bucket_id)));
  }
 private:
  Profile* profile_;
};

// Mapper for 'bookmarks.remove'.
class RemoveBookmarksBucketMapper : public BookmarkBucketMapper<std::string> {
 public:
  explicit RemoveBookmarksBucketMapper(Profile* profile) : profile_(profile) {}
  virtual void GetBucketsForArgs(const ListValue* args,
                                 BucketList* buckets) OVERRIDE {
    typedef std::list<int64> IdList;
    IdList ids;
    bool invalid_id = false;
    if (!BookmarksRemoveFunction::ExtractIds(args, &ids, &invalid_id) ||
        invalid_id) {
      return;
    }

    for (IdList::iterator it = ids.begin(); it != ids.end(); ++it) {
      BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile_);
      const BookmarkNode* node = model->GetNodeByID(*it);
      if (!node || node->is_root())
        return;

      std::string bucket_id;
      bucket_id += UTF16ToUTF8(node->parent()->GetTitle());
      bucket_id += UTF16ToUTF8(node->GetTitle());
      bucket_id += node->url().spec();
      buckets->push_back(GetBucket(base::SHA1HashString(bucket_id)));
    }
  }
 private:
  Profile* profile_;
};

// Mapper for any bookmark function accepting bookmark IDs as parameters, where
// a distinct ID corresponds to a single item in terms of quota limiting.  This
// is inappropriate for bookmarks.remove, for example, since repeated removals
// of the same item will actually have a different ID each time.
template <class FunctionType>
class BookmarkIdMapper : public BookmarkBucketMapper<int64> {
 public:
  typedef std::list<int64> IdList;
  virtual void GetBucketsForArgs(const ListValue* args, BucketList* buckets) {
    IdList ids;
    bool invalid_id = false;
    if (!FunctionType::ExtractIds(args, &ids, &invalid_id) || invalid_id)
      return;
    for (IdList::iterator it = ids.begin(); it != ids.end(); ++it)
      buckets->push_back(GetBucket(*it));
  }
};

// Builds heuristics for all BookmarkFunctions using specialized BucketMappers.
class BookmarksQuotaLimitFactory {
 public:
  // For id-based bookmark functions.
  template <class FunctionType>
  static void Build(QuotaLimitHeuristics* heuristics) {
    BuildWithMappers(heuristics, new BookmarkIdMapper<FunctionType>(),
                                 new BookmarkIdMapper<FunctionType>());
  }

  // For bookmarks.create.
  static void BuildForCreate(QuotaLimitHeuristics* heuristics,
                             Profile* profile) {
    BuildWithMappers(heuristics, new CreateBookmarkBucketMapper(profile),
                                 new CreateBookmarkBucketMapper(profile));
  }

  // For bookmarks.remove.
  static void BuildForRemove(QuotaLimitHeuristics* heuristics,
                             Profile* profile) {
    BuildWithMappers(heuristics, new RemoveBookmarksBucketMapper(profile),
                                 new RemoveBookmarksBucketMapper(profile));
  }

 private:
  static void BuildWithMappers(QuotaLimitHeuristics* heuristics,
      BucketMapper* short_mapper, BucketMapper* long_mapper) {
    const Config kSustainedLimitConfig = {
      // See bookmarks.json for current value.
      bookmarks::MAX_SUSTAINED_WRITE_OPERATIONS_PER_MINUTE,
      TimeDelta::FromMinutes(1)
    };
    heuristics->push_back(new SustainedLimit(
        TimeDelta::FromMinutes(10),
        kSustainedLimitConfig,
        short_mapper,
        "MAX_SUSTAINED_WRITE_OPERATIONS_PER_MINUTE"));

    const Config kTimedLimitConfig = {
      // See bookmarks.json for current value.
      bookmarks::MAX_WRITE_OPERATIONS_PER_HOUR,
      TimeDelta::FromHours(1)
    };
    heuristics->push_back(new TimedLimit(
        kTimedLimitConfig,
        long_mapper,
        "MAX_WRITE_OPERATIONS_PER_HOUR"));
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(BookmarksQuotaLimitFactory);
};

// And finally, building the individual heuristics for each function.
void BookmarksRemoveFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  BookmarksQuotaLimitFactory::BuildForRemove(heuristics, profile());
}

void BookmarksMoveFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  BookmarksQuotaLimitFactory::Build<BookmarksMoveFunction>(heuristics);
}

void BookmarksUpdateFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  BookmarksQuotaLimitFactory::Build<BookmarksUpdateFunction>(heuristics);
};

void BookmarksCreateFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  BookmarksQuotaLimitFactory::BuildForCreate(heuristics, profile());
}

BookmarksIOFunction::BookmarksIOFunction() {}

BookmarksIOFunction::~BookmarksIOFunction() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void BookmarksIOFunction::SelectFile(ui::SelectFileDialog::Type type) {
  // GetDefaultFilepathForBookmarkExport() might have to touch the filesystem
  // (stat or access, for example), so this requires a thread with IO allowed.
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&BookmarksIOFunction::SelectFile, this, type));
    return;
  }

  // Pre-populating the filename field in case this is a SELECT_SAVEAS_FILE
  // dialog. If not, there is no filename field in the dialog box.
  base::FilePath default_path;
  if (type == ui::SelectFileDialog::SELECT_SAVEAS_FILE)
    default_path = GetDefaultFilepathForBookmarkExport();
  else
    DCHECK(type == ui::SelectFileDialog::SELECT_OPEN_FILE);

  // After getting the |default_path|, ask the UI to display the file dialog.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&BookmarksIOFunction::ShowSelectFileDialog, this,
                 type, default_path));
}

void BookmarksIOFunction::ShowSelectFileDialog(
    ui::SelectFileDialog::Type type,
    const base::FilePath& default_path) {
  // Balanced in one of the three callbacks of SelectFileDialog:
  // either FileSelectionCanceled, MultiFilesSelected, or FileSelected
  AddRef();

  WebContents* web_contents = dispatcher()->delegate()->
      GetAssociatedWebContents();

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_contents));
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("html"));
  // TODO(kinaba): http://crbug.com/140425. Turn file_type_info.support_drive
  // on for saving once Google Drive client on ChromeOS supports it.
  if (type == ui::SelectFileDialog::SELECT_OPEN_FILE)
    file_type_info.support_drive = true;
  // |web_contents| can be NULL (for background pages), which is fine. In such
  // a case if file-selection dialogs are forbidden by policy, we will not
  // show an InfoBar, which is better than letting one appear out of the blue.
  select_file_dialog_->SelectFile(type,
                                  string16(),
                                  default_path,
                                  &file_type_info,
                                  0,
                                  FILE_PATH_LITERAL(""),
                                  NULL,
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

bool BookmarksImportFunction::RunImpl() {
  if (!EditBookmarksEnabled())
    return false;
  SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE);
  return true;
}

void BookmarksImportFunction::FileSelected(const base::FilePath& path,
                                           int index,
                                           void* params) {
#if !defined(OS_ANDROID)
  // Android does not have support for the standard importers.
  // TODO(jgreenwald): remove ifdef once extensions are no longer built on
  // Android.
  scoped_refptr<ImporterHost> importer_host(new ImporterHost);
  importer::SourceProfile source_profile;
  source_profile.importer_type = importer::TYPE_BOOKMARKS_FILE;
  source_profile.source_path = path;
  importer_host->StartImportSettings(source_profile,
                                     profile(),
                                     importer::FAVORITES,
                                     new ProfileWriter(profile()),
                                     true);
#endif
  Release();  // Balanced in BookmarksIOFunction::SelectFile()
}

bool BookmarksExportFunction::RunImpl() {
  SelectFile(ui::SelectFileDialog::SELECT_SAVEAS_FILE);
  return true;
}

void BookmarksExportFunction::FileSelected(const base::FilePath& path,
                                           int index,
                                           void* params) {
#if !defined(OS_ANDROID)
  // Android does not have support for the standard exporter.
  // TODO(jgreenwald): remove ifdef once extensions are no longer built on
  // Android.
  bookmark_html_writer::WriteBookmarks(profile(), path, NULL);
#endif
  Release();  // Balanced in BookmarksIOFunction::SelectFile()
}

}  // namespace extensions
