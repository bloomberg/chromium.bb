// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/android/bookmarks_handler.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/favicon_source.h"
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

// Parses a bookmark ID passed back from the NTP.  The IDs differ from the
// normal int64 bookmark ID because we prepend a "p" if the ID represents a
// partner bookmark.
bool ParseNtpBookmarkId(const ListValue* args,
                        int64* out_id,
                        bool* out_is_partner) {
  if (!args || args->empty())
    return false;

  std::string string_id;
  if (!args->GetString(0, &string_id))
    return false;

  if (string_id.empty())
    return false;

  if (StartsWithASCII(string_id, "p", true)) {
    *out_is_partner = true;
    return base::StringToInt64(string_id.substr(1), out_id);
  }

  *out_is_partner = false;
  return base::StringToInt64(string_id, out_id);
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
    partner_bookmarks_shim_ = PartnerBookmarksShim::GetInstance();
    partner_bookmarks_shim_->AddObserver(this);
  }

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
  if (!BookmarkModelFactory::GetForProfile(profile)->IsLoaded())
    return;  // is handled in Loaded().

  // Attach the Partner Bookmarks shim under the Mobile Bookmarks.
  // Cannot do this earlier because mobile_node is not yet set.
  DCHECK(partner_bookmarks_shim_ != NULL);
  if (bookmark_model_) {
    partner_bookmarks_shim_->AttachTo(
        bookmark_model_, bookmark_model_->mobile_node());
  }
  if (!partner_bookmarks_shim_->IsLoaded())
    return;  // is handled with a PartnerShimLoaded() callback

  int64 id;
  bool is_partner;
  if (ParseNtpBookmarkId(args, &id, &is_partner))
    QueryBookmarkFolder(id, is_partner);
  else
    QueryInitialBookmarks();
}

void BookmarksHandler::HandleDeleteBookmark(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  int64 id;
  bool is_partner;
  if (!ParseNtpBookmarkId(args, &id, &is_partner))
    return;

  const BookmarkNode* node =
      partner_bookmarks_shim_->GetNodeByID(id, is_partner);
  if (!partner_bookmarks_shim_->IsBookmarkEditable(node)) {
    NOTREACHED();
    return;
  }

  const BookmarkNode* parent_node = node->parent();
  bookmark_model_->Remove(parent_node, parent_node->GetIndexOf(node));
}

void BookmarksHandler::HandleEditBookmark(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  int64 id;
  bool is_partner;
  if (!ParseNtpBookmarkId(args, &id, &is_partner))
    return;

  const BookmarkNode* node =
      partner_bookmarks_shim_->GetNodeByID(id, is_partner);
  if (!partner_bookmarks_shim_->IsBookmarkEditable(node)) {
    NOTREACHED();
    return;
  }

  TabAndroid* tab = TabAndroid::FromWebContents(web_ui()->GetWebContents());
  if (tab)
    tab->EditBookmark(node->id(), node->is_folder());
}

std::string BookmarksHandler::GetBookmarkIdForNtp(const BookmarkNode* node) {
  return (partner_bookmarks_shim_->IsPartnerBookmark(node) ? "p" : "") +
      Int64ToString(node->id());
}

void BookmarksHandler::SetParentInBookmarksResult(const BookmarkNode* parent,
                                                  DictionaryValue* result) {
  result->SetString(kParentIdParam, GetBookmarkIdForNtp(parent));
}

void BookmarksHandler::PopulateBookmark(const BookmarkNode* node,
                                        ListValue* result) {
  if (!result)
    return;

  DictionaryValue* filler_value = new DictionaryValue();
  filler_value->SetString("title", node->GetTitle());
  // Mark reserved system nodes and partner bookmarks as uneditable
  // (i.e. the bookmark bar along with the "Other Bookmarks" folder).
  filler_value->SetBoolean("editable",
                           partner_bookmarks_shim_->IsBookmarkEditable(node));
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
  ListValue* bookmarks = new ListValue();

  for (int i = 0; i < folder->child_count(); i++) {
    const BookmarkNode* bookmark= folder->GetChild(i);
    PopulateBookmark(bookmark, bookmarks);
  }

  // Make sure we iterate over the partner's attach point
  DCHECK(partner_bookmarks_shim_ != NULL);
  if (partner_bookmarks_shim_->HasPartnerBookmarks() &&
      folder == partner_bookmarks_shim_->get_attach_point()) {
    PopulateBookmark(
        partner_bookmarks_shim_->GetPartnerBookmarksRoot(), bookmarks);
  }

  ListValue* folder_hierarchy = new ListValue();
  const BookmarkNode* parent = partner_bookmarks_shim_->GetParentOf(folder);

  while (parent != NULL) {
    DictionaryValue* hierarchy_entry = new DictionaryValue();
    if (partner_bookmarks_shim_->IsRootNode(parent))
      hierarchy_entry->SetBoolean("root", true);

    hierarchy_entry->SetString("title", parent->GetTitle());
    hierarchy_entry->SetString("id", GetBookmarkIdForNtp(parent));
    folder_hierarchy->Append(hierarchy_entry);
    parent = partner_bookmarks_shim_->GetParentOf(parent);
  }

  result->SetString("title", folder->GetTitle());
  result->SetString("id", GetBookmarkIdForNtp(folder));
  result->SetBoolean("root", partner_bookmarks_shim_->IsRootNode(folder));
  result->Set("bookmarks", bookmarks);
  result->Set("hierarchy", folder_hierarchy);
}

void BookmarksHandler::QueryBookmarkFolder(const int64& folder_id,
                                           bool is_partner_bookmark) {
  DCHECK(partner_bookmarks_shim_ != NULL);
  const BookmarkNode* bookmarks =
      partner_bookmarks_shim_->GetNodeByID(folder_id, is_partner_bookmark);
  if (bookmarks) {
    DictionaryValue result;
    PopulateBookmarksInFolder(bookmarks, &result);
    SendResult(result);
  } else {
    // If we receive an ID that no longer maps to a bookmark folder, just
    // return the initial bookmark folder.
    QueryInitialBookmarks();
  }
}

void BookmarksHandler::QueryInitialBookmarks() {
  DictionaryValue result;
  DCHECK(partner_bookmarks_shim_ != NULL);
  PopulateBookmarksInFolder(
      // We have to go to the partner Root if it exists
      partner_bookmarks_shim_->HasPartnerBookmarks() ?
          partner_bookmarks_shim_->GetPartnerBookmarksRoot() :
          bookmark_model_->mobile_node(),
      &result);
  SendResult(result);
}

void BookmarksHandler::SendResult(const DictionaryValue& result) {
  web_ui()->CallJavascriptFunction("ntp.bookmarks", result);
}

void BookmarksHandler::Loaded(BookmarkModel* model, bool ids_reassigned) {
  BookmarkModelChanged();
}

void BookmarksHandler::PartnerShimLoaded(PartnerBookmarksShim* shim) {
  BookmarkModelChanged();
}

void BookmarksHandler::ShimBeingDeleted(PartnerBookmarksShim* shim) {
  partner_bookmarks_shim_ = NULL;
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

  int64 id;
  bool is_partner;
  if (!ParseNtpBookmarkId(args, &id, &is_partner))
    return;

  DCHECK(partner_bookmarks_shim_ != NULL);
  const BookmarkNode* node =
      partner_bookmarks_shim_->GetNodeByID(id, is_partner);
  if (!node)
    return;

  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);
  favicon_service->GetRawFaviconForURL(
      FaviconService::FaviconForURLParams(
          profile,
          node->url(),
          history::TOUCH_PRECOMPOSED_ICON | history::TOUCH_ICON |
              history::FAVICON,
          0),  // request the largest icon.
      ui::SCALE_FACTOR_100P,  // density doesn't matter for the largest icon.
      base::Bind(&BookmarksHandler::OnShortcutFaviconDataAvailable,
                 base::Unretained(this),
                 node),
      &cancelable_task_tracker_);
}

void BookmarksHandler::OnShortcutFaviconDataAvailable(
    const BookmarkNode* node,
    const history::FaviconBitmapResult& bitmap_result) {
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
    tab->AddShortcutToBookmark(node->url(), node->GetTitle(),
                               favicon_bitmap, SkColorGetR(color),
                               SkColorGetG(color), SkColorGetB(color));
  }
}
