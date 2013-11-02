// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/android/bookmarks_handler.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/favicon_size.h"

using base::Int64ToString;
using content::BrowserThread;

namespace {

static const char* kParentIdParam = "parent_id";
static const char* kNodeIdParam = "node_id";

// Defines actions taken by the user over the partner bookmarks on NTP for
// NewTabPage.BookmarkActionAndroid histogram.
// Should be kept in sync with the values in histograms.xml.
enum PartnerBookmarkAction {
  BOOKMARK_ACTION_DELETE_BOOKMARK_PARTNER = 0,
  BOOKMARK_ACTION_DELETE_ROOT_FOLDER_PARTNER = 1,
  BOOKMARK_ACTION_EDIT_BOOKMARK_PARTNER = 2,
  BOOKMARK_ACTION_EDIT_ROOT_FOLDER_PARTNER = 3,
  BOOKMARK_ACTION_BUCKET_BOUNDARY = 4
};

// Helper to record a bookmark action in BookmarkActionAndroid histogram.
void RecordBookmarkAction(PartnerBookmarkAction type) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.BookmarkActionAndroid", type,
                            BOOKMARK_ACTION_BUCKET_BOUNDARY);
}

std::string BookmarkTypeAsString(BookmarkNode::Type type) {
  switch (type) {
    case BookmarkNode::URL:
      return "URL";
    case BookmarkNode::FOLDER:
      return "FOLDER";
    case BookmarkNode::BOOKMARK_BAR:
      return "BOOKMARK_BAR";
    case BookmarkNode::OTHER_NODE:
      return "OTHER_NODE";
    case BookmarkNode::MOBILE:
      return "MOBILE";
    default:
      return "UNKNOWN";
  }
}

SkColor GetDominantColorForFavicon(scoped_refptr<base::RefCountedMemory> png) {
  color_utils::GridSampler sampler;
  // 100 here is the darkness_limit which represents the minimum sum of the RGB
  // components that is acceptable as a color choice. This can be from 0 to 765.
  // 665 here is the brightness_limit represents the maximum sum of the RGB
  // components that is acceptable as a color choice. This can be from 0 to 765.
  return color_utils::CalculateKMeanColorOfPNG(png, 100, 665, &sampler);
}

}  // namespace

BookmarksHandler::BookmarksHandler()
    : bookmark_model_(NULL),
      partner_bookmarks_shim_(NULL),
      bookmark_data_requested_(false),
      extensive_changes_(false) {
}

BookmarksHandler::~BookmarksHandler() {
  if (bookmark_model_)
    bookmark_model_->RemoveObserver(this);

  if (partner_bookmarks_shim_)
    partner_bookmarks_shim_->RemoveObserver(this);

  if (managed_bookmarks_shim_)
    managed_bookmarks_shim_->RemoveObserver(this);
}

void BookmarksHandler::RegisterMessages() {
  // Listen for the bookmark change. We need the both bookmark and folder
  // change, the NotificationService is not sufficient.
  Profile* profile = Profile::FromBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext());

  content::URLDataSource::Add(
      profile, new FaviconSource(profile, FaviconSource::ANY));

  bookmark_model_ = BookmarkModelFactory::GetForProfile(profile);
  if (bookmark_model_) {
    bookmark_model_->AddObserver(this);
    // Since a sync or import could have started before this class is
    // initialized, we need to make sure that our initial state is
    // up to date.
    extensive_changes_ = bookmark_model_->IsDoingExtensiveChanges();
  }

  // Create the partner Bookmarks shim as early as possible (but don't attach).
  if (!partner_bookmarks_shim_) {
    partner_bookmarks_shim_ = PartnerBookmarksShim::BuildForBrowserContext(
        chrome::GetBrowserContextRedirectedInIncognito(
            web_ui()->GetWebContents()->GetBrowserContext()));
    partner_bookmarks_shim_->AddObserver(this);
  }

  managed_bookmarks_shim_.reset(new ManagedBookmarksShim(profile->GetPrefs()));
  managed_bookmarks_shim_->AddObserver(this);

  // Register ourselves as the handler for the bookmark javascript callbacks.
  web_ui()->RegisterMessageCallback("getBookmarks",
      base::Bind(&BookmarksHandler::HandleGetBookmarks,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("deleteBookmark",
      base::Bind(&BookmarksHandler::HandleDeleteBookmark,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("editBookmark",
        base::Bind(&BookmarksHandler::HandleEditBookmark,
                   base::Unretained(this)));
  web_ui()->RegisterMessageCallback("createHomeScreenBookmarkShortcut",
      base::Bind(&BookmarksHandler::HandleCreateHomeScreenBookmarkShortcut,
                 base::Unretained(this)));
}

void BookmarksHandler::HandleGetBookmarks(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  bookmark_data_requested_ = true;
  Profile* profile = Profile::FromBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext());
  if (!BookmarkModelFactory::GetForProfile(profile)->loaded())
    return;  // is handled in Loaded().

  if (!partner_bookmarks_shim_->IsLoaded())
    return;  // is handled with a PartnerShimLoaded() callback

  const BookmarkNode* node = GetNodeByID(args);
  if (node)
    QueryBookmarkFolder(node);
  else
    QueryInitialBookmarks();
}

void BookmarksHandler::HandleDeleteBookmark(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const BookmarkNode* node = GetNodeByID(args);
  if (!node)
    return;

  if (!IsEditable(node)) {
    NOTREACHED();
    return;
  }

  if (partner_bookmarks_shim_->IsPartnerBookmark(node)) {
    if (partner_bookmarks_shim_->GetPartnerBookmarksRoot() == node)
      RecordBookmarkAction(BOOKMARK_ACTION_DELETE_ROOT_FOLDER_PARTNER);
    else
      RecordBookmarkAction(BOOKMARK_ACTION_DELETE_BOOKMARK_PARTNER);
    partner_bookmarks_shim_->RemoveBookmark(node);
    return;
  }

  const BookmarkNode* parent_node = node->parent();
  bookmark_model_->Remove(parent_node, parent_node->GetIndexOf(node));
}

void BookmarksHandler::HandleEditBookmark(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const BookmarkNode* node = GetNodeByID(args);
  if (!node)
    return;

  if (!IsEditable(node)) {
    NOTREACHED();
    return;
  }

  TabAndroid* tab = TabAndroid::FromWebContents(web_ui()->GetWebContents());
  if (tab) {
    if (partner_bookmarks_shim_->IsPartnerBookmark(node)) {
      if (partner_bookmarks_shim_->GetPartnerBookmarksRoot() == node)
        RecordBookmarkAction(BOOKMARK_ACTION_EDIT_ROOT_FOLDER_PARTNER);
      else
        RecordBookmarkAction(BOOKMARK_ACTION_EDIT_BOOKMARK_PARTNER);
    }
    tab->EditBookmark(node->id(),
                      GetTitle(node),
                      node->is_folder(),
                      partner_bookmarks_shim_->IsPartnerBookmark(node));
  }
}

std::string BookmarksHandler::GetBookmarkIdForNtp(const BookmarkNode* node) {
  std::string prefix;
  if (partner_bookmarks_shim_->IsPartnerBookmark(node))
    prefix = "p";
  else if (managed_bookmarks_shim_->IsManagedBookmark(node))
    prefix = "m";
  return prefix + Int64ToString(node->id());
}

void BookmarksHandler::SetParentInBookmarksResult(const BookmarkNode* parent,
                                                  DictionaryValue* result) {
  result->SetString(kParentIdParam, GetBookmarkIdForNtp(parent));
}

void BookmarksHandler::PopulateBookmark(const BookmarkNode* node,
                                        ListValue* result) {
  if (!result)
    return;

  if (!IsReachable(node))
    return;

  DictionaryValue* filler_value = new DictionaryValue();
  filler_value->SetString("title", GetTitle(node));
  filler_value->SetBoolean("editable", IsEditable(node));
  if (node->is_url()) {
    filler_value->SetBoolean("folder", false);
    filler_value->SetString("url", node->url().spec());
  } else {
    filler_value->SetBoolean("folder", true);
  }
  filler_value->SetString("id", GetBookmarkIdForNtp(node));
  filler_value->SetString("type", BookmarkTypeAsString(node->type()));
  result->Append(filler_value);
}

void BookmarksHandler::PopulateBookmarksInFolder(
    const BookmarkNode* folder,
    DictionaryValue* result) {
  DCHECK(partner_bookmarks_shim_ != NULL);
  DCHECK(IsReachable(folder));

  ListValue* bookmarks = new ListValue();

  // If this is the Mobile bookmarks folder then add the "Managed bookmarks"
  // folder first, so that it's the first entry.
  if (bookmark_model_ && folder == bookmark_model_->mobile_node() &&
      managed_bookmarks_shim_->HasManagedBookmarks()) {
    PopulateBookmark(managed_bookmarks_shim_->GetManagedBookmarksRoot(),
                     bookmarks);
  }

  for (int i = 0; i < folder->child_count(); i++) {
    const BookmarkNode* bookmark= folder->GetChild(i);
    PopulateBookmark(bookmark, bookmarks);
  }

  // Make sure we iterate over the partner's attach point
  if (bookmark_model_ && folder == bookmark_model_->mobile_node() &&
      partner_bookmarks_shim_->HasPartnerBookmarks()) {
    PopulateBookmark(partner_bookmarks_shim_->GetPartnerBookmarksRoot(),
                     bookmarks);
  }

  ListValue* folder_hierarchy = new ListValue();
  const BookmarkNode* parent = GetParentOf(folder);

  while (parent != NULL) {
    DictionaryValue* hierarchy_entry = new DictionaryValue();
    if (IsRoot(parent))
      hierarchy_entry->SetBoolean("root", true);

    hierarchy_entry->SetString("title", GetTitle(parent));
    hierarchy_entry->SetString("id", GetBookmarkIdForNtp(parent));
    folder_hierarchy->Append(hierarchy_entry);
    parent = GetParentOf(parent);
  }

  result->SetString("title", GetTitle(folder));
  result->SetString("id", GetBookmarkIdForNtp(folder));
  result->SetBoolean("root", IsRoot(folder));
  result->Set("bookmarks", bookmarks);
  result->Set("hierarchy", folder_hierarchy);
}

void BookmarksHandler::QueryBookmarkFolder(const BookmarkNode* node) {
  if (node->is_folder() && IsReachable(node)) {
    DictionaryValue result;
    PopulateBookmarksInFolder(node, &result);
    SendResult(result);
  } else {
    // If we receive an ID that no longer maps to a bookmark folder, just
    // return the initial bookmark folder.
    QueryInitialBookmarks();
  }
}

void BookmarksHandler::QueryInitialBookmarks() {
  DictionaryValue result;
  PopulateBookmarksInFolder(bookmark_model_->mobile_node(), &result);
  SendResult(result);
}

void BookmarksHandler::SendResult(const DictionaryValue& result) {
  web_ui()->CallJavascriptFunction("ntp.bookmarks", result);
}

void BookmarksHandler::Loaded(BookmarkModel* model, bool ids_reassigned) {
  BookmarkModelChanged();
}

void BookmarksHandler::PartnerShimChanged(PartnerBookmarksShim* shim) {
  BookmarkModelChanged();
}

void BookmarksHandler::PartnerShimLoaded(PartnerBookmarksShim* shim) {
  BookmarkModelChanged();
}

void BookmarksHandler::ShimBeingDeleted(PartnerBookmarksShim* shim) {
  partner_bookmarks_shim_ = NULL;
}

void BookmarksHandler::OnManagedBookmarksChanged() {
  BookmarkModelChanged();
}

void BookmarksHandler::ExtensiveBookmarkChangesBeginning(BookmarkModel* model) {
  extensive_changes_ = true;
}

void BookmarksHandler::ExtensiveBookmarkChangesEnded(BookmarkModel* model) {
  extensive_changes_ = false;
  BookmarkModelChanged();
}

void BookmarksHandler::BookmarkNodeRemoved(BookmarkModel* model,
                                           const BookmarkNode* parent,
                                           int old_index,
                                           const BookmarkNode* node) {
  DictionaryValue result;
  SetParentInBookmarksResult(parent, &result);
  result.SetString(kNodeIdParam, Int64ToString(node->id()));
  NotifyModelChanged(result);
}

void BookmarksHandler::BookmarkAllNodesRemoved(BookmarkModel* model) {
  if (bookmark_data_requested_ && !extensive_changes_)
    web_ui()->CallJavascriptFunction("ntp.bookmarkChanged");
}

void BookmarksHandler::BookmarkNodeAdded(
    BookmarkModel* model, const BookmarkNode* parent, int index) {
  DictionaryValue result;
  SetParentInBookmarksResult(parent, &result);
  NotifyModelChanged(result);
}

void BookmarksHandler::BookmarkNodeChanged(BookmarkModel* model,
                                           const BookmarkNode* node) {
  DCHECK(partner_bookmarks_shim_);
  DCHECK(!partner_bookmarks_shim_->IsPartnerBookmark(node));
  DictionaryValue result;
  SetParentInBookmarksResult(node->parent(), &result);
  result.SetString(kNodeIdParam, Int64ToString(node->id()));
  NotifyModelChanged(result);
}

void BookmarksHandler::BookmarkModelChanged() {
  if (bookmark_data_requested_ && !extensive_changes_)
    web_ui()->CallJavascriptFunction("ntp.bookmarkChanged");
}

void BookmarksHandler::NotifyModelChanged(const DictionaryValue& status) {
  if (bookmark_data_requested_ && !extensive_changes_)
    web_ui()->CallJavascriptFunction("ntp.bookmarkChanged", status);
}

void BookmarksHandler::HandleCreateHomeScreenBookmarkShortcut(
    const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Profile* profile = Profile::FromBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext());
  if (!profile)
    return;

  DCHECK(partner_bookmarks_shim_ != NULL);
  const BookmarkNode* node = GetNodeByID(args);
  if (!node)
    return;

  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);
  favicon_service->GetRawFaviconForURL(
      FaviconService::FaviconForURLParams(
          profile,
          node->url(),
          chrome::TOUCH_PRECOMPOSED_ICON | chrome::TOUCH_ICON |
              chrome::FAVICON,
          0),  // request the largest icon.
      ui::SCALE_FACTOR_100P,  // density doesn't matter for the largest icon.
      base::Bind(&BookmarksHandler::OnShortcutFaviconDataAvailable,
                 base::Unretained(this),
                 node),
      &cancelable_task_tracker_);
}

void BookmarksHandler::OnShortcutFaviconDataAvailable(
    const BookmarkNode* node,
    const chrome::FaviconBitmapResult& bitmap_result) {
  SkColor color = SK_ColorWHITE;
  SkBitmap favicon_bitmap;
  if (bitmap_result.is_valid()) {
    color = GetDominantColorForFavicon(bitmap_result.bitmap_data);
    gfx::PNGCodec::Decode(bitmap_result.bitmap_data->front(),
                          bitmap_result.bitmap_data->size(),
                          &favicon_bitmap);
  }
  TabAndroid* tab = TabAndroid::FromWebContents(web_ui()->GetWebContents());
  if (tab) {
    tab->AddShortcutToBookmark(node->url(),
                               GetTitle(node),
                               favicon_bitmap, SkColorGetR(color),
                               SkColorGetG(color), SkColorGetB(color));
  }
}

const BookmarkNode* BookmarksHandler::GetNodeByID(
    const base::ListValue* args) const {
  // Parses a bookmark ID passed back from the NTP.  The IDs differ from the
  // normal int64 bookmark ID because we prepend a "p" if the ID represents a
  // partner bookmark, and an "m" if the ID represents a managed bookmark.

  if (!args || args->empty())
    return NULL;

  std::string string_id;
  if (!args->GetString(0, &string_id) || string_id.empty()) {
    NOTREACHED();
    return NULL;
  }

  bool is_partner = string_id[0] == 'p';
  bool is_managed = string_id[0] == 'm';

  if (is_partner || is_managed)
    string_id = string_id.substr(1);

  int64 id = 0;
  if (!base::StringToInt64(string_id, &id)) {
    NOTREACHED();
    return NULL;
  }

  if (is_managed)
    return managed_bookmarks_shim_->GetNodeByID(id);

  if (is_partner)
    return partner_bookmarks_shim_->GetNodeByID(id);

  return bookmark_model_->GetNodeByID(id);
}

const BookmarkNode* BookmarksHandler::GetParentOf(
    const BookmarkNode* node) const {
  if (node == managed_bookmarks_shim_->GetManagedBookmarksRoot() ||
      node == partner_bookmarks_shim_->GetPartnerBookmarksRoot()) {
    return bookmark_model_->mobile_node();
  }

  return node->parent();
}

base::string16 BookmarksHandler::GetTitle(const BookmarkNode* node) const {
  if (partner_bookmarks_shim_->IsPartnerBookmark(node))
    return partner_bookmarks_shim_->GetTitle(node);

  return node->GetTitle();
}

bool BookmarksHandler::IsReachable(const BookmarkNode* node) const {
  if (!partner_bookmarks_shim_->IsPartnerBookmark(node))
    return true;

  return partner_bookmarks_shim_->IsReachable(node);
}

bool BookmarksHandler::IsEditable(const BookmarkNode* node) const {
  // Reserved system nodes and managed bookmarks are not editable.
  // Additionally, bookmark editing may be completely disabled
  // via a managed preference.
  if (!node ||
      (node->type() != BookmarkNode::FOLDER &&
          node->type() != BookmarkNode::URL)) {
    return false;
  }

  const PrefService* pref = Profile::FromBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext())->GetPrefs();
  if (!pref->GetBoolean(prefs::kEditBookmarksEnabled))
    return false;

  if (partner_bookmarks_shim_->IsPartnerBookmark(node))
    return true;

  return !managed_bookmarks_shim_->IsManagedBookmark(node);
}

bool BookmarksHandler::IsRoot(const BookmarkNode* node) const {
  return node->is_root() &&
         node != partner_bookmarks_shim_->GetPartnerBookmarksRoot() &&
         node != managed_bookmarks_shim_->GetManagedBookmarksRoot();
}
