// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/mediaplayer_ui.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/dom_ui_favicon_source.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_job.h"

#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

static const std::wstring kPropertyPath = L"path";
static const std::wstring kPropertyForce = L"force";
static const std::wstring kPropertyTitle = L"title";
static const std::wstring kPropertyOffset = L"currentOffset";

const char* kMediaplayerURL = "chrome://mediaplayer";
const char* kMediaplayerPlaylistURL = "chrome://mediaplayer#playlist";
const int kPopupLeft = 0;
const int kPopupTop = 0;
const int kPopupWidth = 350;
const int kPopupHeight = 300;

class MediaplayerUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  explicit MediaplayerUIHTMLSource(bool is_playlist);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~MediaplayerUIHTMLSource() {}
  bool is_playlist_;

  DISALLOW_COPY_AND_ASSIGN(MediaplayerUIHTMLSource);
};

// The handler for Javascript messages related to the "mediaplayer" view.
class MediaplayerHandler : public DOMMessageHandler,
                           public base::SupportsWeakPtr<MediaplayerHandler> {
 public:
  explicit MediaplayerHandler(bool is_playlist);
  virtual ~MediaplayerHandler();

  // Init work after Attach.
  void Init(bool is_playlist, TabContents* contents);

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

  // Callback for the "currentOffsetChanged" message.
  void HandleCurrentOffsetChanged(const Value* value);

  void PlaybackMediaFile(const GURL& url);

  void EnqueueMediaFile(const GURL& url);

  void GetPlaylistValue(ListValue& value);

  // Callback for the "playbackError" message.
  void HandlePlaybackError(const Value* value);

  // Callback for the "getCurrentPlaylist" message.
  void HandleGetCurrentPlaylist(const Value* value);

  void HandleTogglePlaylist(const Value* value);
  void HandleSetCurrentPlaylistOffset(const Value* value);

  const std::vector<GURL>& GetCurrentPlaylist();

  int GetCurrentPlaylistOffset();
  void SetCurrentPlaylistOffset(int offset);
  // Used to set the playlist for playlist views, since the playlist is
  // maintained by the mediaplayer itself.
  void SetCurrentPlaylist(const std::vector<GURL>& playlist, int offset);

 private:

  Profile* profile_;
  std::vector<GURL> currentPlaylist_;
  int currentOffset_;
  bool is_playlist_;
  DISALLOW_COPY_AND_ASSIGN(MediaplayerHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// MediaplayerHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

MediaplayerUIHTMLSource::MediaplayerUIHTMLSource(bool is_playlist)
    : DataSource(chrome::kChromeUIMediaplayerHost, MessageLoop::current()) {
  is_playlist_ = is_playlist;
}

void MediaplayerUIHTMLSource::StartDataRequest(const std::string& path,
    bool is_off_the_record, int request_id) {
  DictionaryValue localized_strings;
  // TODO(dhg): Add stirings to localized strings, also add more strings
  // that are currently hardcoded.
  localized_strings.SetString(L"devices", "devices");

  SetFontAndTextDirection(&localized_strings);

  std::string full_html;

  static const base::StringPiece mediaplayer_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_MEDIAPLAYER_HTML));

  static const base::StringPiece playlist_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_MEDIAPLAYERPLAYLIST_HTML));

  if (is_playlist_) {
    full_html = jstemplate_builder::GetI18nTemplateHtml(
        playlist_html, &localized_strings);
  } else {
    full_html = jstemplate_builder::GetI18nTemplateHtml(
        mediaplayer_html, &localized_strings);
  }

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

////////////////////////////////////////////////////////////////////////////////
//
// MediaplayerHandler
//
////////////////////////////////////////////////////////////////////////////////
MediaplayerHandler::MediaplayerHandler(bool is_playlist)
    : profile_(NULL),
      currentOffset_(0),
      is_playlist_(is_playlist) {
}

MediaplayerHandler::~MediaplayerHandler() {
}

DOMMessageHandler* MediaplayerHandler::Attach(DOMUI* dom_ui) {
  // Create our favicon data source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(new DOMUIFavIconSource(dom_ui->GetProfile()))));
  profile_ = dom_ui->GetProfile();

  return DOMMessageHandler::Attach(dom_ui);
}

void MediaplayerHandler::Init(bool is_playlist, TabContents* contents) {
  MediaPlayer* player = MediaPlayer::Get();
  if (!is_playlist) {
    player->RegisterNewHandler(this, contents);
  } else {
    player->RegisterNewPlaylistHandler(this, contents);
  }
}

void MediaplayerHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("currentOffsetChanged",
      NewCallback(this, &MediaplayerHandler::HandleCurrentOffsetChanged));
  dom_ui_->RegisterMessageCallback("playbackError",
      NewCallback(this, &MediaplayerHandler::HandlePlaybackError));
  dom_ui_->RegisterMessageCallback("getCurrentPlaylist",
      NewCallback(this, &MediaplayerHandler::HandleGetCurrentPlaylist));
  dom_ui_->RegisterMessageCallback("togglePlaylist",
      NewCallback(this, &MediaplayerHandler::HandleTogglePlaylist));
  dom_ui_->RegisterMessageCallback("setCurrentPlaylistOffset",
      NewCallback(this, &MediaplayerHandler::HandleSetCurrentPlaylistOffset));
}

void MediaplayerHandler::GetPlaylistValue(ListValue& value) {
  for (unsigned int x = 0; x < currentPlaylist_.size(); x++) {
    DictionaryValue* url_value = new DictionaryValue();
    url_value->SetString(kPropertyPath, currentPlaylist_[x].spec());
    value.Append(url_value);
  }
}

void MediaplayerHandler::PlaybackMediaFile(const GURL& url) {
  GURL newurl(url);
  currentPlaylist_.clear();
  currentPlaylist_.push_back(newurl);

  DictionaryValue info_value;
  ListValue urls;
  GetPlaylistValue(urls);
  info_value.SetBoolean(kPropertyForce, true);
  info_value.SetString(kPropertyPath, newurl.spec());
  dom_ui_->CallJavascriptFunction(L"playlistChanged", info_value, urls);
  MediaPlayer::Get()->NotifyPlaylistChanged();
}

const std::vector<GURL>& MediaplayerHandler::GetCurrentPlaylist() {
  return currentPlaylist_;
}

int MediaplayerHandler::GetCurrentPlaylistOffset() {
  return currentOffset_;
}

void MediaplayerHandler::HandleSetCurrentPlaylistOffset(const Value* value) {
  ListValue results_value;
  DictionaryValue info_value;
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    std::string val;

    // Get the new playlist offset;
    if (list_value->GetString(0, &val)) {
      int id = atoi(val.c_str());
      MediaPlayer::Get()->SetPlaylistOffset(id);
    }
  }
}

void MediaplayerHandler::SetCurrentPlaylistOffset(int offset) {
  currentOffset_ = offset;
  DictionaryValue info_value;
  ListValue urls;
  GetPlaylistValue(urls);
  info_value.SetString(kPropertyPath, "");
  info_value.SetBoolean(kPropertyForce, true);
  info_value.SetInteger(kPropertyOffset, currentOffset_);
  dom_ui_->CallJavascriptFunction(L"playlistChanged", info_value, urls);
}

void MediaplayerHandler::SetCurrentPlaylist(const std::vector<GURL>& playlist,
                                            int offset) {
  currentPlaylist_ = playlist;
  currentOffset_ = offset;
  DictionaryValue info_value;
  ListValue urls;
  GetPlaylistValue(urls);
  info_value.SetString(kPropertyPath, "");
  info_value.SetBoolean(kPropertyForce, false);
  info_value.SetInteger(kPropertyOffset, currentOffset_);
  dom_ui_->CallJavascriptFunction(L"playlistChanged", info_value, urls);
}

void MediaplayerHandler::EnqueueMediaFile(const GURL& url) {
  GURL newurl(url);
  currentPlaylist_.push_back(newurl);
  DictionaryValue info_value;
  ListValue urls;
  GetPlaylistValue(urls);
  info_value.SetString(kPropertyPath, newurl.spec());
  info_value.SetBoolean(kPropertyForce, false);
  info_value.SetInteger(kPropertyOffset, currentOffset_);
  dom_ui_->CallJavascriptFunction(L"playlistChanged", info_value, urls);
  MediaPlayer::Get()->NotifyPlaylistChanged();
}

void MediaplayerHandler::HandleCurrentOffsetChanged(const Value* value) {
  ListValue results_value;
  DictionaryValue info_value;
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    std::string val;

    // Get the new playlist offset;
    if (list_value->GetString(0, &val)) {
      int id = atoi(val.c_str());
      currentOffset_ = id;
      MediaPlayer::Get()->NotifyPlaylistChanged();
    }
  }
}

void MediaplayerHandler::HandlePlaybackError(const Value* value) {
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    std::string error;

    // Get path string.
    if (list_value->GetString(0, &error)) {
      LOG(ERROR) << "Playback error" << error;
    }
  }
}

void MediaplayerHandler::HandleGetCurrentPlaylist(const Value* value) {
  DictionaryValue info_value;
  ListValue urls;
  GetPlaylistValue(urls);
  info_value.SetInteger(kPropertyOffset, currentOffset_);
  dom_ui_->CallJavascriptFunction(L"playlistChanged", info_value, urls);
}

void MediaplayerHandler::HandleTogglePlaylist(const Value* value) {
  MediaPlayer::Get()->TogglePlaylistWindowVisible();
}

////////////////////////////////////////////////////////////////////////////////
//
// Mediaplayer
//
////////////////////////////////////////////////////////////////////////////////

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
template <>
struct RunnableMethodTraits<MediaPlayer> {
  void RetainCallee(MediaPlayer* obj) {}
  void ReleaseCallee(MediaPlayer* obj) {}
};

void MediaPlayer::EnqueueMediaURL(const GURL& url) {
  if (handler_ == NULL) {
    GURL newurl(url);
    unhandled_urls_.push_back(url);
    PopupMediaPlayer();
  } else {
    handler_->EnqueueMediaFile(url);
  }
}

void MediaPlayer::ForcePlayMediaURL(const GURL& url) {
  if (handler_ == NULL) {
    GURL newurl(url);
    unhandled_urls_.push_back(url);
    PopupMediaPlayer();
  } else {
    handler_->PlaybackMediaFile(url);
  }
}

void MediaPlayer::TogglePlaylistWindowVisible() {
  if (playlist_browser_) {
    ClosePlaylistWindow();
  } else {
    ShowPlaylistWindow();
  }
}

void MediaPlayer::ShowPlaylistWindow() {
  if (playlist_browser_ == NULL) {
    PopupPlaylist();
  }
}

void MediaPlayer::ClosePlaylistWindow() {
  if (playlist_browser_ != NULL) {
    playlist_browser_->window()->Close();
  }
}

void MediaPlayer::SetPlaylistOffset(int offset) {
  if (handler_) {
    handler_->SetCurrentPlaylistOffset(offset);
  }
  if (playlist_) {
    playlist_->SetCurrentPlaylistOffset(offset);
  }
}

void MediaPlayer::RegisterNewHandler(MediaplayerHandler* handler,
                                     TabContents* contents) {
  handler_ = handler;
  mediaplayer_tab_ = contents;
  RegisterListeners();
  if (unhandled_urls_.size() != 0) {
    for (unsigned int x = 0; x < unhandled_urls_.size(); x++) {
      handler_->EnqueueMediaFile(unhandled_urls_[x]);
    }
    unhandled_urls_.clear();
  }
}

void MediaPlayer::RegisterListeners() {
  registrar_.RemoveAll();
  if (playlist_tab_) {
    registrar_.Add(this,
                   NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(playlist_tab_));
  }
  if (mediaplayer_tab_) {
    registrar_.Add(this,
                   NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(mediaplayer_tab_));
  }
};

void MediaPlayer::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
  if (Source<TabContents>(source).ptr() == mediaplayer_tab_) {
    RemoveHandler(handler_);
    RegisterListeners();
    ClosePlaylistWindow();
  } else if (Source<TabContents>(source).ptr() == playlist_tab_) {
    RemovePlaylistHandler(playlist_);
    RegisterListeners();
  }
}

void MediaPlayer::RegisterNewPlaylistHandler(
    MediaplayerHandler* handler, TabContents* contents) {
  playlist_ = handler;
  playlist_tab_ = contents;
  RegisterListeners();
  NotifyPlaylistChanged();
}

void MediaPlayer::RemovePlaylistHandler(MediaplayerHandler* handler) {
  if (handler == playlist_) {
    playlist_ = NULL;
    playlist_browser_ = NULL;
    playlist_tab_ = NULL;
  }
}

void MediaPlayer::NotifyPlaylistChanged() {
  if (handler_ && playlist_) {
    playlist_->SetCurrentPlaylist(handler_->GetCurrentPlaylist(),
                                  handler_->GetCurrentPlaylistOffset());
  }
}

void MediaPlayer::RemoveHandler(MediaplayerHandler* handler) {
  if (handler == handler_) {
    handler_ = NULL;
    mediaplayer_browser_ = NULL;
    mediaplayer_tab_ = NULL;
  }
}

void MediaPlayer::PopupPlaylist() {
  playlist_browser_ = Browser::CreateForPopup(profile_);
  playlist_browser_->AddTabWithURL(
      GURL(kMediaplayerPlaylistURL), GURL(), PageTransition::LINK,
      true, -1, false, NULL);
  playlist_browser_->window()->SetBounds(gfx::Rect(kPopupLeft,
                                                   kPopupTop,
                                                   kPopupWidth,
                                                   kPopupHeight),
                                         BrowserWindow::WINDOW_BOUNDS);
  playlist_browser_->window()->Show();
}

void MediaPlayer::PopupMediaPlayer() {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &MediaPlayer::PopupMediaPlayer));
    return;
  }
  mediaplayer_browser_ = Browser::CreateForPopup(profile_);
  mediaplayer_browser_->AddTabWithURL(
      GURL(kMediaplayerURL), GURL(), PageTransition::LINK,
      true, -1, false, NULL);
  mediaplayer_browser_->window()->SetBounds(gfx::Rect(kPopupLeft,
                                                      kPopupTop,
                                                      kPopupWidth,
                                                      kPopupHeight),
                                            BrowserWindow::WINDOW_BOUNDS);
  mediaplayer_browser_->window()->Show();
}

URLRequestJob* MediaPlayer::MaybeIntercept(URLRequest* request) {
  // Don't attempt to intercept here as we want to wait until the mime
  // type is fully determined.
  return NULL;
}

// This is the list of mime types currently supported by the Google
// Document Viewer.
static const char* const supported_mime_type_list[] = {
  "audio/mpeg",
  "video/mp4",
  "audio/mp3"
};

URLRequestJob* MediaPlayer::MaybeInterceptResponse(
    URLRequest* request) {
  // Do not intercept this request if it is a download.
  if (request->load_flags() & net::LOAD_IS_DOWNLOAD) {
    return NULL;
  }

  std::string mime_type;
  request->GetMimeType(&mime_type);
  // If it is in our list of known URLs, enqueue the url then
  // Cancel the request so the mediaplayer can handle it when
  // it hits it in the playlist.
  if (supported_mime_types_.count(mime_type) > 0) {
    if (request->referrer() != "chrome://mediaplayer" &&
        !request->referrer().empty()) {
      EnqueueMediaURL(request->url());
      request->Cancel();
    }
  }
  return NULL;
}

MediaPlayer::MediaPlayer()
    : handler_(NULL),
      playlist_(NULL),
      playlist_browser_(NULL),
      mediaplayer_browser_(NULL),
      mediaplayer_tab_(NULL),
      playlist_tab_(NULL) {
  URLRequest::RegisterRequestInterceptor(this);
  for (size_t i = 0; i < arraysize(supported_mime_type_list); ++i) {
    supported_mime_types_.insert(supported_mime_type_list[i]);
  }
};

////////////////////////////////////////////////////////////////////////////////
//
// MediaplayerUIContents
//
////////////////////////////////////////////////////////////////////////////////

MediaplayerUI::MediaplayerUI(TabContents* contents) : DOMUI(contents) {
  const GURL& url = contents->GetURL();
  bool is_playlist = (url.ref() == "playlist");
  MediaplayerHandler* handler = new MediaplayerHandler(is_playlist);
  AddMessageHandler((handler)->Attach(this));
  if (is_playlist) {
    handler->Init(true, contents);
  } else {
    handler->Init(false, contents);
  }

  MediaplayerUIHTMLSource* html_source =
      new MediaplayerUIHTMLSource(is_playlist);

  // Set up the chrome://mediaplayer/ source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}
