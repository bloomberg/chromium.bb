// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_html_writer.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_codec.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/favicon_size.h"

using content::BrowserThread;

namespace {

static BookmarkFaviconFetcher* fetcher = NULL;

// File header.
const char kHeader[] =
    "<!DOCTYPE NETSCAPE-Bookmark-file-1>\r\n"
    "<!-- This is an automatically generated file.\r\n"
    "     It will be read and overwritten.\r\n"
    "     DO NOT EDIT! -->\r\n"
    "<META HTTP-EQUIV=\"Content-Type\""
    " CONTENT=\"text/html; charset=UTF-8\">\r\n"
    "<TITLE>Bookmarks</TITLE>\r\n"
    "<H1>Bookmarks</H1>\r\n"
    "<DL><p>\r\n";

// Newline separator.
const char kNewline[] = "\r\n";

// The following are used for bookmarks.

// Start of a bookmark.
const char kBookmarkStart[] = "<DT><A HREF=\"";
// After kBookmarkStart.
const char kAddDate[] = "\" ADD_DATE=\"";
// After kAddDate.
const char kIcon[] = "\" ICON=\"";
// After kIcon.
const char kBookmarkAttributeEnd[] = "\">";
// End of a bookmark.
const char kBookmarkEnd[] = "</A>";

// The following are used when writing folders.

// Start of a folder.
const char kFolderStart[] = "<DT><H3 ADD_DATE=\"";
// After kFolderStart.
const char kLastModified[] = "\" LAST_MODIFIED=\"";
// After kLastModified when writing the bookmark bar.
const char kBookmarkBar[] = "\" PERSONAL_TOOLBAR_FOLDER=\"true\">";
// After kLastModified when writing a user created folder.
const char kFolderAttributeEnd[] = "\">";
// End of the folder.
const char kFolderEnd[] = "</H3>";
// Start of the children of a folder.
const char kFolderChildren[] = "<DL><p>";
// End of the children for a folder.
const char kFolderChildrenEnd[] = "</DL><p>";

// Number of characters to indent by.
const size_t kIndentSize = 4;

// Class responsible for the actual writing. Takes ownership of favicons_map.
class Writer : public base::RefCountedThreadSafe<Writer> {
 public:
  Writer(base::Value* bookmarks,
         const base::FilePath& path,
         BookmarkFaviconFetcher::URLFaviconMap* favicons_map,
         BookmarksExportObserver* observer)
      : bookmarks_(bookmarks),
        path_(path),
        favicons_map_(favicons_map),
        observer_(observer) {
  }

  // Writing bookmarks and favicons data to file.
  void DoWrite() {
    if (!OpenFile())
      return;

    Value* roots = NULL;
    if (!Write(kHeader) ||
        bookmarks_->GetType() != Value::TYPE_DICTIONARY ||
        !static_cast<DictionaryValue*>(bookmarks_.get())->Get(
            BookmarkCodec::kRootsKey, &roots) ||
        roots->GetType() != Value::TYPE_DICTIONARY) {
      NOTREACHED();
      return;
    }

    DictionaryValue* roots_d_value = static_cast<DictionaryValue*>(roots);
    Value* root_folder_value;
    Value* other_folder_value = NULL;
    Value* mobile_folder_value = NULL;
    if (!roots_d_value->Get(BookmarkCodec::kRootFolderNameKey,
                            &root_folder_value) ||
        root_folder_value->GetType() != Value::TYPE_DICTIONARY ||
        !roots_d_value->Get(BookmarkCodec::kOtherBookmarkFolderNameKey,
                            &other_folder_value) ||
        other_folder_value->GetType() != Value::TYPE_DICTIONARY ||
        !roots_d_value->Get(BookmarkCodec::kMobileBookmarkFolderNameKey,
                            &mobile_folder_value) ||
        mobile_folder_value->GetType() != Value::TYPE_DICTIONARY) {
      NOTREACHED();
      return;  // Invalid type for root folder and/or other folder.
    }

    IncrementIndent();

    if (!WriteNode(*static_cast<DictionaryValue*>(root_folder_value),
                   BookmarkNode::BOOKMARK_BAR) ||
        !WriteNode(*static_cast<DictionaryValue*>(other_folder_value),
                   BookmarkNode::OTHER_NODE) ||
        !WriteNode(*static_cast<DictionaryValue*>(mobile_folder_value),
                   BookmarkNode::MOBILE)) {
      return;
    }

    DecrementIndent();

    Write(kFolderChildrenEnd);
    Write(kNewline);
    // File stream close is forced so that unit test could read it.
    file_stream_.reset();

    NotifyOnFinish();
  }

 private:
  friend class base::RefCountedThreadSafe<Writer>;

  // Types of text being written out. The type dictates how the text is
  // escaped.
  enum TextType {
    // The text is the value of an html attribute, eg foo in
    // <a href="foo">.
    ATTRIBUTE_VALUE,

    // Actual content, eg foo in <h1>foo</h2>.
    CONTENT
  };

  ~Writer() {}

  // Opens the file, returning true on success.
  bool OpenFile() {
    file_stream_.reset(new net::FileStream(NULL));
    int flags = base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE;
    return (file_stream_->OpenSync(path_, flags) == net::OK);
  }

  // Increments the indent.
  void IncrementIndent() {
    indent_.resize(indent_.size() + kIndentSize, ' ');
  }

  // Decrements the indent.
  void DecrementIndent() {
    DCHECK(!indent_.empty());
    indent_.resize(indent_.size() - kIndentSize, ' ');
  }

  // Called at the end of the export process.
  void NotifyOnFinish() {
    if (observer_ != NULL) {
      observer_->OnExportFinished();
    }
  }

  // Writes raw text out returning true on success. This does not escape
  // the text in anyway.
  bool Write(const std::string& text) {
    // net::FileStream does not allow 0-byte writes.
    if (!text.length())
      return true;
    size_t wrote = file_stream_->WriteSync(text.c_str(), text.length());
    bool result = (wrote == text.length());
    DCHECK(result);
    return result;
  }

  // Writes out the text string (as UTF8). The text is escaped based on
  // type.
  bool Write(const std::string& text, TextType type) {
    DCHECK(IsStringUTF8(text));
    std::string utf8_string;

    switch (type) {
      case ATTRIBUTE_VALUE:
        // Convert " to &quot;
        utf8_string = text;
        ReplaceSubstringsAfterOffset(&utf8_string, 0, "\"", "&quot;");
        break;

      case CONTENT:
        utf8_string = net::EscapeForHTML(text);
        break;

      default:
        NOTREACHED();
    }

    return Write(utf8_string);
  }

  // Indents the current line.
  bool WriteIndent() {
    return Write(indent_);
  }

  // Converts a time string written to the JSON codec into a time_t string
  // (used by bookmarks.html) and writes it.
  bool WriteTime(const std::string& time_string) {
    int64 internal_value;
    base::StringToInt64(time_string, &internal_value);
    return Write(base::Int64ToString(
        base::Time::FromInternalValue(internal_value).ToTimeT()));
  }

  // Writes the node and all its children, returning true on success.
  bool WriteNode(const DictionaryValue& value,
                BookmarkNode::Type folder_type) {
    std::string title, date_added_string, type_string;
    if (!value.GetString(BookmarkCodec::kNameKey, &title) ||
        !value.GetString(BookmarkCodec::kDateAddedKey, &date_added_string) ||
        !value.GetString(BookmarkCodec::kTypeKey, &type_string) ||
        (type_string != BookmarkCodec::kTypeURL &&
         type_string != BookmarkCodec::kTypeFolder))  {
      NOTREACHED();
      return false;
    }

    if (type_string == BookmarkCodec::kTypeURL) {
      std::string url_string;
      if (!value.GetString(BookmarkCodec::kURLKey, &url_string)) {
        NOTREACHED();
        return false;
      }

      std::string favicon_string;
      BookmarkFaviconFetcher::URLFaviconMap::iterator itr =
          favicons_map_->find(url_string);
      if (itr != favicons_map_->end()) {
        scoped_refptr<base::RefCountedMemory> data(itr->second.get());
        std::string favicon_data;
        favicon_data.assign(reinterpret_cast<const char*>(data->front()),
                            data->size());
        std::string favicon_base64_encoded;
        if (base::Base64Encode(favicon_data, &favicon_base64_encoded)) {
          GURL favicon_url("data:image/png;base64," + favicon_base64_encoded);
          favicon_string = favicon_url.spec();
        }
      }

      if (!WriteIndent() ||
          !Write(kBookmarkStart) ||
          !Write(url_string, ATTRIBUTE_VALUE) ||
          !Write(kAddDate) ||
          !WriteTime(date_added_string) ||
          (!favicon_string.empty() &&
              (!Write(kIcon) ||
               !Write(favicon_string, ATTRIBUTE_VALUE))) ||
          !Write(kBookmarkAttributeEnd) ||
          !Write(title, CONTENT) ||
          !Write(kBookmarkEnd) ||
          !Write(kNewline)) {
        return false;
      }
      return true;
    }

    // Folder.
    std::string last_modified_date;
    const Value* child_values = NULL;
    if (!value.GetString(BookmarkCodec::kDateModifiedKey,
                         &last_modified_date) ||
        !value.Get(BookmarkCodec::kChildrenKey, &child_values) ||
        child_values->GetType() != Value::TYPE_LIST) {
      NOTREACHED();
      return false;
    }
    if (folder_type != BookmarkNode::OTHER_NODE &&
        folder_type != BookmarkNode::MOBILE) {
      // The other/mobile folder name are not written out. This gives the effect
      // of making the contents of the 'other folder' be a sibling to the
      // bookmark bar folder.
      if (!WriteIndent() ||
          !Write(kFolderStart) ||
          !WriteTime(date_added_string) ||
          !Write(kLastModified) ||
          !WriteTime(last_modified_date)) {
        return false;
      }
      if (folder_type == BookmarkNode::BOOKMARK_BAR) {
        if (!Write(kBookmarkBar))
          return false;
        title = l10n_util::GetStringUTF8(IDS_BOOKMARK_BAR_FOLDER_NAME);
      } else if (!Write(kFolderAttributeEnd)) {
        return false;
      }
      if (!Write(title, CONTENT) ||
          !Write(kFolderEnd) ||
          !Write(kNewline) ||
          !WriteIndent() ||
          !Write(kFolderChildren) ||
          !Write(kNewline)) {
        return false;
      }
      IncrementIndent();
    }

    // Write the children.
    const ListValue* children = static_cast<const ListValue*>(child_values);
    for (size_t i = 0; i < children->GetSize(); ++i) {
      const Value* child_value;
      if (!children->Get(i, &child_value) ||
          child_value->GetType() != Value::TYPE_DICTIONARY) {
        NOTREACHED();
        return false;
      }
      if (!WriteNode(*static_cast<const DictionaryValue*>(child_value),
                     BookmarkNode::FOLDER)) {
        return false;
      }
    }
    if (folder_type != BookmarkNode::OTHER_NODE &&
        folder_type != BookmarkNode::MOBILE) {
      // Close out the folder.
      DecrementIndent();
      if (!WriteIndent() ||
          !Write(kFolderChildrenEnd) ||
          !Write(kNewline)) {
        return false;
      }
    }
    return true;
  }

  // The BookmarkModel as a Value. This value was generated from the
  // BookmarkCodec.
  scoped_ptr<Value> bookmarks_;

  // Path we're writing to.
  base::FilePath path_;

  // Map that stores favicon per URL.
  scoped_ptr<BookmarkFaviconFetcher::URLFaviconMap> favicons_map_;

  // Observer to be notified on finish.
  BookmarksExportObserver* observer_;

  // File we're writing to.
  scoped_ptr<net::FileStream> file_stream_;

  // How much we indent when writing a bookmark/folder. This is modified
  // via IncrementIndent and DecrementIndent.
  std::string indent_;

  DISALLOW_COPY_AND_ASSIGN(Writer);
};

}  // namespace

BookmarkFaviconFetcher::BookmarkFaviconFetcher(
    Profile* profile,
    const base::FilePath& path,
    BookmarksExportObserver* observer)
    : profile_(profile),
      path_(path),
      observer_(observer) {
  favicons_map_.reset(new URLFaviconMap());
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::Source<Profile>(profile_));
}

BookmarkFaviconFetcher::~BookmarkFaviconFetcher() {
}

void BookmarkFaviconFetcher::ExportBookmarks() {
  ExtractUrls(BookmarkModelFactory::GetForProfile(
      profile_)->bookmark_bar_node());
  ExtractUrls(BookmarkModelFactory::GetForProfile(profile_)->other_node());
  ExtractUrls(BookmarkModelFactory::GetForProfile(profile_)->mobile_node());
  if (!bookmark_urls_.empty())
    FetchNextFavicon();
  else
    ExecuteWriter();
}

void BookmarkFaviconFetcher::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (chrome::NOTIFICATION_PROFILE_DESTROYED == type && fetcher != NULL) {
    MessageLoop::current()->DeleteSoon(FROM_HERE, fetcher);
    fetcher = NULL;
  }
}

void BookmarkFaviconFetcher::ExtractUrls(const BookmarkNode* node) {
  if (node->is_url()) {
    std::string url = node->url().spec();
    if (!url.empty())
      bookmark_urls_.push_back(url);
  } else {
    for (int i = 0; i < node->child_count(); ++i)
      ExtractUrls(node->GetChild(i));
  }
}

void BookmarkFaviconFetcher::ExecuteWriter() {
  // BookmarkModel isn't thread safe (nor would we want to lock it down
  // for the duration of the write), as such we make a copy of the
  // BookmarkModel using BookmarkCodec then write from that.
  BookmarkCodec codec;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&Writer::DoWrite,
                 new Writer(codec.Encode(BookmarkModelFactory::GetForProfile(
                                profile_)),
                            path_, favicons_map_.release(), observer_)));
  if (fetcher != NULL) {
    MessageLoop::current()->DeleteSoon(FROM_HERE, fetcher);
    fetcher = NULL;
  }
}

bool BookmarkFaviconFetcher::FetchNextFavicon() {
  if (bookmark_urls_.empty()) {
    return false;
  }
  do {
    std::string url = bookmark_urls_.front();
    // Filter out urls that we've already got favicon for.
    URLFaviconMap::const_iterator iter = favicons_map_->find(url);
    if (favicons_map_->end() == iter) {
      FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
          profile_, Profile::EXPLICIT_ACCESS);
      favicon_service->GetRawFaviconForURL(
          FaviconService::FaviconForURLParams(
              profile_, GURL(url), history::FAVICON, gfx::kFaviconSize),
          ui::SCALE_FACTOR_100P,
          base::Bind(&BookmarkFaviconFetcher::OnFaviconDataAvailable,
                     base::Unretained(this)),
          &cancelable_task_tracker_);
      return true;
    } else {
      bookmark_urls_.pop_front();
    }
  } while (!bookmark_urls_.empty());
  return false;
}

void BookmarkFaviconFetcher::OnFaviconDataAvailable(
    const history::FaviconBitmapResult& bitmap_result) {
  GURL url;
  if (!bookmark_urls_.empty()) {
    url = GURL(bookmark_urls_.front());
    bookmark_urls_.pop_front();
  }
  if (bitmap_result.is_valid() && !url.is_empty()) {
    favicons_map_->insert(
        make_pair(url.spec(), bitmap_result.bitmap_data));
  }

  if (FetchNextFavicon()) {
    return;
  }
  ExecuteWriter();
}

namespace bookmark_html_writer {

void WriteBookmarks(Profile* profile,
                    const base::FilePath& path,
                    BookmarksExportObserver* observer) {
  // BookmarkModel isn't thread safe (nor would we want to lock it down
  // for the duration of the write), as such we make a copy of the
  // BookmarkModel using BookmarkCodec then write from that.
  if (fetcher == NULL) {
    fetcher = new BookmarkFaviconFetcher(profile, path, observer);
    fetcher->ExportBookmarks();
  }
}

}  // namespace bookmark_html_writer
