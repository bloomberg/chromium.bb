// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_bookmarks_module.h"

#include "base/json/json_writer.h"
#include "base/sha1.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_codec.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_bookmarks_module_constants.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extensions_quota_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

namespace keys = extension_bookmarks_module_constants;

using base::TimeDelta;
typedef QuotaLimitHeuristic::Bucket Bucket;
typedef QuotaLimitHeuristic::Config Config;
typedef QuotaLimitHeuristic::BucketList BucketList;
typedef ExtensionsQuotaService::TimedLimit TimedLimit;
typedef ExtensionsQuotaService::SustainedLimit SustainedLimit;
typedef QuotaLimitHeuristic::BucketMapper BucketMapper;

// Helper functions.
class ExtensionBookmarks {
 public:
  // Convert |node| into a JSON value.
  static DictionaryValue* GetNodeDictionary(const BookmarkNode* node,
                                            bool recurse) {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString(keys::kIdKey, Int64ToString(node->id()));

    const BookmarkNode* parent = node->GetParent();
    if (parent) {
      dict->SetString(keys::kParentIdKey, Int64ToString(parent->id()));
      dict->SetInteger(keys::kIndexKey, parent->IndexOfChild(node));
    }

    if (!node->is_folder()) {
      dict->SetString(keys::kUrlKey, node->GetURL().spec());
    } else {
      // Javascript Date wants milliseconds since the epoch, ToDoubleT is
      // seconds.
      base::Time t = node->date_group_modified();
      if (!t.is_null())
        dict->SetReal(keys::kDateGroupModifiedKey, floor(t.ToDoubleT() * 1000));
    }

    dict->SetString(keys::kTitleKey, node->GetTitle());
    if (!node->date_added().is_null()) {
      // Javascript Date wants milliseconds since the epoch, ToDoubleT is
      // seconds.
      dict->SetReal(keys::kDateAddedKey,
                    floor(node->date_added().ToDoubleT() * 1000));
    }

    if (recurse && node->is_folder()) {
      int childCount = node->GetChildCount();
      ListValue* children = new ListValue();
      for (int i = 0; i < childCount; ++i) {
        const BookmarkNode* child = node->GetChild(i);
        DictionaryValue* dict = GetNodeDictionary(child, true);
        children->Append(dict);
      }
      dict->Set(keys::kChildrenKey, children);
    }
    return dict;
  }

  // Add a JSON representation of |node| to the JSON |list|.
  static void AddNode(const BookmarkNode* node, ListValue* list, bool recurse) {
    DictionaryValue* dict = GetNodeDictionary(node, recurse);
    list->Append(dict);
  }

  static bool RemoveNode(BookmarkModel* model, int64 id, bool recursive,
                         std::string* error) {
    const BookmarkNode* node = model->GetNodeByID(id);
    if (!node) {
      *error = keys::kNoNodeError;
      return false;
    }
    if (node == model->root_node() ||
        node == model->other_node() ||
        node == model->GetBookmarkBarNode()) {
      *error = keys::kModifySpecialError;
      return false;
    }
    if (node->is_folder() && node->GetChildCount() > 0 && !recursive) {
      *error = keys::kFolderNotEmptyError;
      return false;
    }

    const BookmarkNode* parent = node->GetParent();
    int index = parent->IndexOfChild(node);
    model->Remove(parent, index);
    return true;
  }

 private:
  ExtensionBookmarks();
};

void BookmarksFunction::Run() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  if (!model->IsLoaded()) {
    // Bookmarks are not ready yet.  We'll wait.
    registrar_.Add(this, NotificationType::BOOKMARK_MODEL_LOADED,
                   NotificationService::AllSources());
    AddRef();  // Balanced in Observe().
    return;
  }

  bool success = RunImpl();
  if (success) {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_BOOKMARKS_API_INVOKED,
        Source<const Extension>(GetExtension()),
        Details<const BookmarksFunction>(this));
  }
  SendResponse(success);
}

bool BookmarksFunction::GetBookmarkIdAsInt64(
    const std::string& id_string, int64* id) {
  if (StringToInt64(id_string, id))
    return true;

  error_ = keys::kInvalidIdError;
  return false;
}

void BookmarksFunction::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  DCHECK(type == NotificationType::BOOKMARK_MODEL_LOADED);
  DCHECK(profile()->GetBookmarkModel()->IsLoaded());
  Run();
  Release();  // Balanced in Run().
}

// static
ExtensionBookmarkEventRouter* ExtensionBookmarkEventRouter::GetSingleton() {
  return Singleton<ExtensionBookmarkEventRouter>::get();
}

ExtensionBookmarkEventRouter::ExtensionBookmarkEventRouter() {
}

ExtensionBookmarkEventRouter::~ExtensionBookmarkEventRouter() {
}

void ExtensionBookmarkEventRouter::Observe(BookmarkModel* model) {
  if (models_.find(model) == models_.end()) {
    model->AddObserver(this);
    models_.insert(model);
  }
}

void ExtensionBookmarkEventRouter::DispatchEvent(Profile *profile,
                                                 const char* event_name,
                                                 const std::string json_args) {
  if (profile->GetExtensionMessageService()) {
    profile->GetExtensionMessageService()->
        DispatchEventToRenderers(event_name, json_args);
  }
}

void ExtensionBookmarkEventRouter::Loaded(BookmarkModel* model) {
  // TODO(erikkay): Perhaps we should send this event down to the extension
  // so they know when it's safe to use the API?
}

void ExtensionBookmarkEventRouter::BookmarkNodeMoved(
    BookmarkModel* model,
    const BookmarkNode* old_parent,
    int old_index,
    const BookmarkNode* new_parent,
    int new_index) {
  ListValue args;
  const BookmarkNode* node = new_parent->GetChild(new_index);
  args.Append(new StringValue(Int64ToString(node->id())));
  DictionaryValue* object_args = new DictionaryValue();
  object_args->SetString(keys::kParentIdKey, Int64ToString(new_parent->id()));
  object_args->SetInteger(keys::kIndexKey, new_index);
  object_args->SetString(keys::kOldParentIdKey,
                         Int64ToString(old_parent->id()));
  object_args->SetInteger(keys::kOldIndexKey, old_index);
  args.Append(object_args);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(model->profile(), keys::kOnBookmarkMoved, json_args);
}

void ExtensionBookmarkEventRouter::BookmarkNodeAdded(BookmarkModel* model,
                                                     const BookmarkNode* parent,
                                                     int index) {
  ListValue args;
  const BookmarkNode* node = parent->GetChild(index);
  args.Append(new StringValue(Int64ToString(node->id())));
  DictionaryValue* obj = ExtensionBookmarks::GetNodeDictionary(node, false);
  args.Append(obj);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(model->profile(), keys::kOnBookmarkCreated, json_args);
}

void ExtensionBookmarkEventRouter::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    int index,
    const BookmarkNode* node) {
  ListValue args;
  args.Append(new StringValue(Int64ToString(node->id())));
  DictionaryValue* object_args = new DictionaryValue();
  object_args->SetString(keys::kParentIdKey, Int64ToString(parent->id()));
  object_args->SetInteger(keys::kIndexKey, index);
  args.Append(object_args);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(model->profile(), keys::kOnBookmarkRemoved, json_args);
}

void ExtensionBookmarkEventRouter::BookmarkNodeChanged(
    BookmarkModel* model, const BookmarkNode* node) {
  ListValue args;
  args.Append(new StringValue(Int64ToString(node->id())));

  // TODO(erikkay) The only two things that BookmarkModel sends this
  // notification for are title and favicon.  Since we're currently ignoring
  // favicon and since the notification doesn't say which one anyway, for now
  // we only include title.  The ideal thing would be to change BookmarkModel
  // to indicate what changed.
  DictionaryValue* object_args = new DictionaryValue();
  object_args->SetString(keys::kTitleKey, node->GetTitle());
  args.Append(object_args);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(model->profile(), keys::kOnBookmarkChanged, json_args);
}

void ExtensionBookmarkEventRouter::BookmarkNodeFavIconLoaded(
    BookmarkModel* model, const BookmarkNode* node) {
  // TODO(erikkay) anything we should do here?
}

void ExtensionBookmarkEventRouter::BookmarkNodeChildrenReordered(
    BookmarkModel* model, const BookmarkNode* node) {
  ListValue args;
  args.Append(new StringValue(Int64ToString(node->id())));
  int childCount = node->GetChildCount();
  ListValue* children = new ListValue();
  for (int i = 0; i < childCount; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    Value* child_id = new StringValue(Int64ToString(child->id()));
    children->Append(child_id);
  }
  DictionaryValue* reorder_info = new DictionaryValue();
  reorder_info->Set(keys::kChildIdsKey, children);
  args.Append(reorder_info);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  DispatchEvent(model->profile(),
                keys::kOnBookmarkChildrenReordered,
                json_args);
}

bool GetBookmarksFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  scoped_ptr<ListValue> json(new ListValue());
  if (args_->IsType(Value::TYPE_LIST)) {
    const ListValue* ids = args_as_list();
    size_t count = ids->GetSize();
    EXTENSION_FUNCTION_VALIDATE(count > 0);
    for (size_t i = 0; i < count; ++i) {
      int64 id;
      std::string id_string;
      EXTENSION_FUNCTION_VALIDATE(ids->GetString(i, &id_string));
      if (!GetBookmarkIdAsInt64(id_string, &id))
        return false;
      const BookmarkNode* node = model->GetNodeByID(id);
      if (!node) {
        error_ = keys::kNoNodeError;
        return false;
      } else {
        ExtensionBookmarks::AddNode(node, json.get(), false);
      }
    }
  } else {
    int64 id;
    std::string id_string;
    EXTENSION_FUNCTION_VALIDATE(args_->GetAsString(&id_string));
    if (!GetBookmarkIdAsInt64(id_string, &id))
      return false;
    const BookmarkNode* node = model->GetNodeByID(id);
    if (!node) {
      error_ = keys::kNoNodeError;
      return false;
    }
    ExtensionBookmarks::AddNode(node, json.get(), false);
  }

  result_.reset(json.release());
  return true;
}

bool GetBookmarkChildrenFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  int64 id;
  std::string id_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsString(&id_string));
  if (!GetBookmarkIdAsInt64(id_string, &id))
    return false;
  scoped_ptr<ListValue> json(new ListValue());
  const BookmarkNode* node = model->GetNodeByID(id);
  if (!node) {
    error_ = keys::kNoNodeError;
    return false;
  }
  int child_count = node->GetChildCount();
  for (int i = 0; i < child_count; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    ExtensionBookmarks::AddNode(child, json.get(), false);
  }

  result_.reset(json.release());
  return true;
}

bool GetBookmarkRecentFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_INTEGER));
  int number_of_items;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&number_of_items));
  if (number_of_items < 1)
    return false;

  BookmarkModel* model = profile()->GetBookmarkModel();
  ListValue* json = new ListValue();
  std::vector<const BookmarkNode*> nodes;
  bookmark_utils::GetMostRecentlyAddedEntries(model, number_of_items, &nodes);
  std::vector<const BookmarkNode*>::iterator i = nodes.begin();
  for (; i != nodes.end(); ++i) {
    const BookmarkNode* node = *i;
    ExtensionBookmarks::AddNode(node, json, false);
  }
  result_.reset(json);
  return true;
}

bool GetBookmarkTreeFunction::RunImpl() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  scoped_ptr<ListValue> json(new ListValue());
  const BookmarkNode* node = model->root_node();
  ExtensionBookmarks::AddNode(node, json.get(), true);
  result_.reset(json.release());
  return true;
}

bool SearchBookmarksFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_STRING));

  std::wstring query;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsString(&query));

  BookmarkModel* model = profile()->GetBookmarkModel();
  ListValue* json = new ListValue();
  std::wstring lang = profile()->GetPrefs()->GetString(prefs::kAcceptLanguages);
  std::vector<const BookmarkNode*> nodes;
  bookmark_utils::GetBookmarksContainingText(model, query, 50, lang, &nodes);
  std::vector<const BookmarkNode*>::iterator i = nodes.begin();
  for (; i != nodes.end(); ++i) {
    const BookmarkNode* node = *i;
    ExtensionBookmarks::AddNode(node, json, false);
  }

  result_.reset(json);
  return true;
}

// static
bool RemoveBookmarkFunction::ExtractIds(const Value* args,
    std::list<int64>* ids, bool* invalid_id) {
  std::string id_string;
  if (args->IsType(Value::TYPE_STRING) &&
      args->GetAsString(&id_string)) {
    int64 id;
    if (StringToInt64(id_string, &id))
      ids->push_back(id);
    else
      *invalid_id = true;
  } else {
    if (!args->IsType(Value::TYPE_LIST))
      return false;
    const ListValue* ids_list = static_cast<const ListValue*>(args);
    size_t count = ids_list->GetSize();
    if (count <= 0)
      return false;
    for (size_t i = 0; i < count; ++i) {
      if (!ids_list->GetString(i, &id_string))
        return false;
      int64 id;
      if (StringToInt64(id_string, &id)) {
        ids->push_back(id);
      } else {
        *invalid_id = true;
        break;
      }
    }
  }
  return true;
}

bool RemoveBookmarkFunction::RunImpl() {
  std::list<int64> ids;
  bool invalid_id = false;
  EXTENSION_FUNCTION_VALIDATE(ExtractIds(args_.get(), &ids, &invalid_id));
  if (invalid_id) {
    error_ = keys::kInvalidIdError;
    return false;
  }
  bool recursive = false;
  if (name() == RemoveTreeBookmarkFunction::function_name())
    recursive = true;

  BookmarkModel* model = profile()->GetBookmarkModel();
  size_t count = ids.size();
  EXTENSION_FUNCTION_VALIDATE(count > 0);
  for (std::list<int64>::iterator it = ids.begin(); it != ids.end(); ++it) {
    if (!ExtensionBookmarks::RemoveNode(model, *it, recursive, &error_))
      return false;
  }
  return true;
}

bool CreateBookmarkFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* json = args_as_dictionary();

  BookmarkModel* model = profile()->GetBookmarkModel();
  int64 parentId;
  if (!json->HasKey(keys::kParentIdKey)) {
    // Optional, default to "other bookmarks".
    parentId = model->other_node()->id();
  } else {
    std::string parentId_string;
    EXTENSION_FUNCTION_VALIDATE(json->GetString(keys::kParentIdKey,
                                                &parentId_string));
    if (!GetBookmarkIdAsInt64(parentId_string, &parentId))
      return false;
  }
  const BookmarkNode* parent = model->GetNodeByID(parentId);
  if (!parent) {
    error_ = keys::kNoParentError;
    return false;
  }
  if (parent->GetParent() == NULL) {  // Can't create children of the root.
    error_ = keys::kNoParentError;
    return false;
  }

  int index;
  if (!json->HasKey(keys::kIndexKey)) {  // Optional (defaults to end).
    index = parent->GetChildCount();
  } else {
    EXTENSION_FUNCTION_VALIDATE(json->GetInteger(keys::kIndexKey, &index));
    if (index > parent->GetChildCount() || index < 0) {
      error_ = keys::kInvalidIndexError;
      return false;
    }
  }

  std::wstring title;
  json->GetString(keys::kTitleKey, &title);  // Optional.
  std::string url_string;
  json->GetString(keys::kUrlKey, &url_string);  // Optional.
  GURL url(url_string);
  if (!url.is_empty() && !url.is_valid()) {
    error_ = keys::kInvalidUrlError;
    return false;
  }

  const BookmarkNode* node;
  if (url_string.length())
    node = model->AddURL(parent, index, title, url);
  else
    node = model->AddGroup(parent, index, title);
  DCHECK(node);
  if (!node) {
    error_ = keys::kNoNodeError;
    return false;
  }

  DictionaryValue* ret = ExtensionBookmarks::GetNodeDictionary(node, false);
  result_.reset(ret);

  return true;
}

// static
bool MoveBookmarkFunction::ExtractIds(const Value* args,
                                      std::list<int64>* ids,
                                      bool* invalid_id) {
  // For now, Move accepts ID parameters in the same way as an Update.
  return UpdateBookmarkFunction::ExtractIds(args, ids, invalid_id);
}

bool MoveBookmarkFunction::RunImpl() {
  std::list<int64> ids;
  bool invalid_id = false;
  EXTENSION_FUNCTION_VALIDATE(ExtractIds(args_.get(), &ids, &invalid_id));
  if (invalid_id) {
    error_ = keys::kInvalidIdError;
    return false;
  }
  EXTENSION_FUNCTION_VALIDATE(ids.size() == 1);
  const ListValue* args = args_as_list();

  DictionaryValue* destination;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &destination));

  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* node = model->GetNodeByID(ids.front());
  if (!node) {
    error_ = keys::kNoNodeError;
    return false;
  }
  if (node == model->root_node() ||
      node == model->other_node() ||
      node == model->GetBookmarkBarNode()) {
    error_ = keys::kModifySpecialError;
    return false;
  }

  const BookmarkNode* parent = NULL;
  if (!destination->HasKey(keys::kParentIdKey)) {
    // Optional, defaults to current parent.
    parent = node->GetParent();
  } else {
    std::string parentId_string;
    EXTENSION_FUNCTION_VALIDATE(destination->GetString(keys::kParentIdKey,
        &parentId_string));
    int64 parentId;
    if (!GetBookmarkIdAsInt64(parentId_string, &parentId))
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
  if (destination->HasKey(keys::kIndexKey)) {  // Optional (defaults to end).
    EXTENSION_FUNCTION_VALIDATE(destination->GetInteger(keys::kIndexKey,
                                                        &index));
    if (index > parent->GetChildCount() || index < 0) {
      error_ = keys::kInvalidIndexError;
      return false;
    }
  } else {
    index = parent->GetChildCount();
  }

  model->Move(node, parent, index);

  DictionaryValue* ret = ExtensionBookmarks::GetNodeDictionary(node, false);
  result_.reset(ret);

  return true;
}

// static
bool UpdateBookmarkFunction::ExtractIds(const Value* args,
                                        std::list<int64>* ids,
                                        bool* invalid_id) {
  std::string id_string;
  if (!args->IsType(Value::TYPE_LIST))
    return false;
  const ListValue* args_list = static_cast<const ListValue*>(args);
  if (!args_list->GetString(0, &id_string))
    return false;
  int64 id;
  if (StringToInt64(id_string, &id))
    ids->push_back(id);
  else
    *invalid_id = true;
  return true;
}

bool UpdateBookmarkFunction::RunImpl() {
  std::list<int64> ids;
  bool invalid_id = false;
  EXTENSION_FUNCTION_VALIDATE(ExtractIds(args_.get(), &ids, &invalid_id));
  if (invalid_id) {
    error_ = keys::kInvalidIdError;
    return false;
  }
  EXTENSION_FUNCTION_VALIDATE(ids.size() == 1);

  const ListValue* args = args_as_list();
  DictionaryValue* updates;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &updates));
  std::wstring title;
  updates->GetString(keys::kTitleKey, &title);  // Optional (empty is clear).

  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* node = model->GetNodeByID(ids.front());
  if (!node) {
    error_ = keys::kNoNodeError;
    return false;
  }
  if (node == model->root_node() ||
      node == model->other_node() ||
      node == model->GetBookmarkBarNode()) {
    error_ = keys::kModifySpecialError;
    return false;
  }
  model->SetTitle(node, title);

  DictionaryValue* ret = ExtensionBookmarks::GetNodeDictionary(node, false);
  result_.reset(ret);

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
  // TODO(tim): This should share code with CreateBookmarkFunction::RunImpl,
  // but I can't figure out a good way to do that with all the macros.
  virtual void GetBucketsForArgs(const Value* args, BucketList* buckets) {
    if (!args->IsType(Value::TYPE_DICTIONARY))
      return;

    std::string parent_id;
    const DictionaryValue* json = static_cast<const DictionaryValue*>(args);
    if (json->HasKey(keys::kParentIdKey)) {
      if (!json->GetString(keys::kParentIdKey, &parent_id))
        return;
    }
    BookmarkModel* model = profile_->GetBookmarkModel();
    const BookmarkNode* parent = model->GetNodeByID(StringToInt64(parent_id));
    if (!parent)
      return;

    std::string bucket_id = WideToUTF8(parent->GetTitle());
    std::wstring title;
    json->GetString(keys::kTitleKey, &title);
    std::string url_string;
    json->GetString(keys::kUrlKey, &url_string);

    bucket_id += WideToUTF8(title);
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
  virtual void GetBucketsForArgs(const Value* args, BucketList* buckets) {
    typedef std::list<int64> IdList;
    IdList ids;
    bool invalid_id = false;
    if (!RemoveBookmarkFunction::ExtractIds(args, &ids, &invalid_id) ||
        invalid_id) {
      return;
    }

    for (IdList::iterator it = ids.begin(); it != ids.end(); ++it) {
      BookmarkModel* model = profile_->GetBookmarkModel();
      const BookmarkNode* node = model->GetNodeByID(*it);
      if (!node || !node->GetParent())
        return;

      std::string bucket_id;
      bucket_id += WideToUTF8(node->GetParent()->GetTitle());
      bucket_id += WideToUTF8(node->GetTitle());
      bucket_id += node->GetURL().spec();
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
  virtual void GetBucketsForArgs(const Value* args, BucketList* buckets) {
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
    TimedLimit* timed = new TimedLimit(kLongLimitConfig, long_mapper);
    // A max of two operations per minute, sustained over 10 minutes.
    SustainedLimit* sustained = new SustainedLimit(TimeDelta::FromMinutes(10),
        kShortLimitConfig, short_mapper);
    heuristics->push_back(timed);
    heuristics->push_back(sustained);
  }

  // The quota configurations used for all BookmarkFunctions.
  static const Config kShortLimitConfig;
  static const Config kLongLimitConfig;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BookmarksQuotaLimitFactory);
};

const Config BookmarksQuotaLimitFactory::kShortLimitConfig = {
  2,                         // 2 tokens per interval.
  TimeDelta::FromMinutes(1)  // 1 minute long refill interval.
};

const Config BookmarksQuotaLimitFactory::kLongLimitConfig = {
  100,                       // 100 tokens per interval.
  TimeDelta::FromHours(1)    // 1 hour long refill interval.
};

// And finally, building the individual heuristics for each function.
void RemoveBookmarkFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  BookmarksQuotaLimitFactory::BuildForRemove(heuristics, profile());
}

void MoveBookmarkFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  BookmarksQuotaLimitFactory::Build<MoveBookmarkFunction>(heuristics);
}

void UpdateBookmarkFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  BookmarksQuotaLimitFactory::Build<UpdateBookmarkFunction>(heuristics);
};

void CreateBookmarkFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  BookmarksQuotaLimitFactory::BuildForCreate(heuristics, profile());
}
