// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/translation_service.h"

#include "base/json/json_reader.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/profile.h"
#include "chrome/common/render_messages.h"
#include "net/base/escape.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/renderer_host/translate/translate_internal.h"
#else
// Defining dummy URLs for unit-tests to pass.
#define TRANSLATE_SERVER_URL "http://disabled"
#define TRANSLATE_SERVER_SECURE_URL "https://disabled"
#endif

namespace {

// The URLs we send translation requests to.
const char kServiceURL[] = TRANSLATE_SERVER_URL;
const char kSecureServiceURL[] = TRANSLATE_SERVER_SECURE_URL;

// The different params used when sending requests to the translate server.
const char kVersionParam[] = "v";
const char kLangPairParam[] = "langpair";
const char kTextParam[] = "q";
const char kClientParam[] = "client";
const char kFormatParam[] = "format";
const char kSSLParam[] = "ssl";
const char kTranslationCountParam[] = "tc";

// Describes languages deemed equivalent from a translation point of view.
// This is used to detect unnecessary translations.
struct LocaleToCLDLanguage {
  const char* locale_language;  // Language Chrome locale is in.
  const char* cld_language;     // Language the CLD reports.
};
LocaleToCLDLanguage kLocaleToCLDLanguages[] = {
    { "en-GB", "en" },
    { "en-US", "en" },
    { "es-419", "es" },
};

// The maximum size in bytes after which the server will refuse the request.
const size_t kTextRequestMaxSize = 1024 * 30;

// Delay to wait for before sending a request to the translation server.
const int kSendRequestDelay = 100;

// Task used to send the current pending translation request for a renderer
// after some time has elapsed with no new request from that renderer.
// Note that this task is canceled when TranslationRequest is destroyed, which
// happens when the TranslationService is going away.  So it is OK to have it
// have a pointer to the TranslationService.
class SendTranslationRequestTask : public CancelableTask {
 public:
  SendTranslationRequestTask(TranslationService* translation_service,
                             int renderer_id,
                             bool secure);
  virtual void Run();
  virtual void Cancel();

 private:
  TranslationService* translation_service_;
  int renderer_id_;
  bool secure_;
  bool canceled_;

  DISALLOW_COPY_AND_ASSIGN(SendTranslationRequestTask);
};

}  // namespace

// Contains the information necessary to send a request to the translation
// server.  It is used to group several renderer queries, as to limit the
// load sent to the translation server.
struct TranslationService::TranslationRequest {
  TranslationRequest(int routing_id,
                     int page_id,
                     const std::string& source_lang,
                     const std::string& target_lang,
                     bool secure)
      : routing_id(routing_id),
        page_id(page_id),
        source_lang(source_lang),
        target_lang(target_lang),
        secure(secure),
        send_query_task(NULL) {
    renderer_request_info.reset(new RendererRequestInfoList());
  }

  ~TranslationRequest() {
    if (send_query_task)
      send_query_task->Cancel();
  }

  void Clear() {
    page_id = 0;
    source_lang.clear();
    target_lang.clear();
    query.clear();
    renderer_request_info->clear();
    if (send_query_task) {
      send_query_task->Cancel();
      send_query_task = NULL;
    }
  }

  int routing_id;
  int page_id;
  std::string source_lang;
  std::string target_lang;
  bool secure;
  std::string query;
  // renderer_request_info is a scoped_ptr so that we avoid copying the list
  // when the request is sent.  At that point we only transfer ownership of that
  // list to renderer_request_infos_.
  scoped_ptr<RendererRequestInfoList> renderer_request_info;
  CancelableTask* send_query_task;
};

////////////////////////////////////////////////////////////////////////////////
// SendTranslationRequestTask

SendTranslationRequestTask::SendTranslationRequestTask(
    TranslationService* translation_service,
    int renderer_id,
    bool secure)
    : translation_service_(translation_service),
      renderer_id_(renderer_id),
      secure_(secure),
      canceled_(false) {
}

void SendTranslationRequestTask::Run() {
  if (canceled_)
    return;
  translation_service_->
      SendTranslationRequestForRenderer(renderer_id_, secure_);
}

void SendTranslationRequestTask::Cancel() {
  canceled_ = true;
}

////////////////////////////////////////////////////////////////////////////////
// TranslationService, public:

TranslationService::TranslationService(IPC::Message::Sender* message_sender)
    : message_sender_(message_sender) {
}

TranslationService::~TranslationService() {
  STLDeleteContainerPairSecondPointers(pending_translation_requests_.begin(),
                                       pending_translation_requests_.end());
  STLDeleteContainerPairSecondPointers(
      pending_secure_translation_requests_.begin(),
      pending_secure_translation_requests_.end());
  STLDeleteContainerPairPointers(renderer_request_infos_.begin(),
                                 renderer_request_infos_.end());
}

void TranslationService::Translate(int routing_id,
                                   int page_id,
                                   int work_id,
                                   const TextChunks& text_chunks,
                                   const std::string& source_lang,
                                   const std::string& target_lang,
                                   bool secure) {
  TranslationRequestMap& request_map =
      secure ? pending_secure_translation_requests_ :
               pending_translation_requests_;
  TranslationRequestMap::iterator iter = request_map.find(routing_id);
  TranslationRequest* translation_request = NULL;

  string16 utf16_text = MergeTextChunks(text_chunks);
  std::string text = EscapeUrlEncodedData(UTF16ToUTF8(utf16_text));

  if (iter != request_map.end()) {
    translation_request = iter->second;
    if (page_id != translation_request->page_id) {
      // We are getting a request from a renderer for a different page id.
      // This indicates we navigated away from the page that was being
      // translated.  We should drop the current pending translations.
      translation_request->Clear();
      // Set the new states.
      translation_request->page_id = page_id;
      translation_request->source_lang = source_lang;
      translation_request->target_lang = target_lang;
    } else {
      DCHECK(translation_request->source_lang == source_lang);
      DCHECK(translation_request->target_lang == target_lang);
      // Cancel the pending tasks to send the query.  We'll be posting a new one
      // after we updated the request.
      translation_request->send_query_task->Cancel();
      translation_request->send_query_task = NULL;
      if (translation_request->query.size() + text.size() >=
          kTextRequestMaxSize) {
        // The request would be too big with that last addition of text, send
        // the request now. (Single requests too big to be sent in 1 translation
        // request are dealt with below.)
        if (!translation_request->query.empty()) {  // Single requests
          SendRequestToTranslationServer(translation_request);
          // The translation request has been deleted.
          translation_request = NULL;
          iter = request_map.end();
        }
      }
    }
  }

  if (translation_request == NULL) {
    translation_request = new TranslationRequest(routing_id, page_id,
                                                 source_lang, target_lang,
                                                 secure);
    request_map[routing_id] = translation_request;
  }

  AddTextToRequestString(&(translation_request->query), text,
                         source_lang, target_lang, secure);

  translation_request->renderer_request_info->push_back(
      RendererRequestInfo(routing_id, work_id));

  if (translation_request->query.size() > kTextRequestMaxSize) {
    DCHECK(translation_request->renderer_request_info->size() == 1U);
    // This one request is too large for the translation service.
    // TODO(jcampan): we should support such requests by splitting them.
    iter = request_map.find(routing_id);
    DCHECK(iter != request_map.end());
    request_map.erase(iter);
    message_sender_->Send(
        new ViewMsg_TranslateTextReponse(routing_id, work_id, 1, TextChunks()));
    delete translation_request;
    return;
  }

  // Now post the new task that will ensure we'll send the request to the
  // translation server if no renderer requests are received within a
  // reasonable amount of time.
  DCHECK(!translation_request->send_query_task);
  translation_request->send_query_task =
      new SendTranslationRequestTask(this, routing_id, secure);
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      translation_request->send_query_task, GetSendRequestDelay());
}

void TranslationService::SendTranslationRequestForRenderer(int renderer_id,
                                                           bool secure) {
  TranslationRequestMap& request_map =
      secure ? pending_secure_translation_requests_ :
               pending_translation_requests_;
  TranslationRequestMap::const_iterator iter = request_map.find(renderer_id);
  DCHECK(iter != request_map.end());
  SendRequestToTranslationServer(iter->second);
}

void TranslationService::OnURLFetchComplete(const URLFetcher* source,
                                            const GURL& url,
                                            const URLRequestStatus& status,
                                            int response_code,
                                            const ResponseCookies& cookies,
                                            const std::string& data) {
  if (!status.is_success() || response_code != 200 || data.empty()) {
    TranslationFailed(source);
    return;
  }

  // If the response is a simple string, put it in an array.  (The JSONReader
  // requires an array or map at the root.)
  std::string str;
  if (data.size() > 1U && data[0] == '"') {
    str.append("[");
    str.append(data);
    str.append("]");
  }
  scoped_ptr<Value> value(base::JSONReader::Read(str.empty() ? data : str,
                                                 true));
  if (!value.get()) {
    NOTREACHED() << "Translation server returned invalid JSON response.";
    TranslationFailed(source);
    return;
  }

  // If the request was for a single string, the response is the translated
  // string.
  TextChunksList translated_chunks_list;
  if (value->IsType(Value::TYPE_STRING)) {
    string16 str16;
    if (!value->GetAsUTF16(&str16)) {
      NOTREACHED();
      TranslationFailed(source);
      return;
    }
    TextChunks text_chunks;
    text_chunks.push_back(str16);
    translated_chunks_list.push_back(text_chunks);
  } else {
    if (!value->IsType(Value::TYPE_LIST)) {
      NOTREACHED() << "Translation server returned unexpected JSON response "
          " (not a list).";
      TranslationFailed(source);
      return;
    }
    ListValue* list = static_cast<ListValue*>(value.get());
    for (size_t i = 0; i < list->GetSize(); ++i) {
      string16 translated_text;
      if (!list->GetStringAsUTF16(i, &translated_text)) {
        NOTREACHED() << "Translation server returned unexpected JSON response "
            " (unexpected type in list).";
        TranslationFailed(source);
        return;
      }
      translated_text = UnescapeForHTML(translated_text);
      TranslationService::TextChunks text_chunks;
      TranslationService::SplitTextChunks(translated_text, &text_chunks);
      translated_chunks_list.push_back(text_chunks);
    }
  }

  // We have successfully extracted all the translated text chunks, send them to
  // the renderer.
  SendResponseToRenderer(source, 0, translated_chunks_list);
}

// static
bool TranslationService::ShouldTranslatePage(
    const std::string& page_language, const std::string& chrome_language) {
  // Most locale names are the actual ISO 639 codes that the Google translate
  // API uses, but for the ones longer than 2 chars.
  // See l10n_util.cc for the list.
  for (size_t i = 0; i < arraysize(kLocaleToCLDLanguages); ++i) {
    if (chrome_language == kLocaleToCLDLanguages[i].locale_language &&
        page_language == kLocaleToCLDLanguages[i].cld_language) {
      return false;
    }
  }
  return true;
}

// static
bool TranslationService::IsTranslationEnabled() {
  return GURL(kServiceURL).host() != "disabled";
}

////////////////////////////////////////////////////////////////////////////////
// TranslationService, protected:

int TranslationService::GetSendRequestDelay() const {
  return kSendRequestDelay;
}

////////////////////////////////////////////////////////////////////////////////
// TranslationService, private:

void TranslationService::SendRequestToTranslationServer(
    TranslationRequest* request) {
  DCHECK(!request->query.empty());
  GURL url(request->secure ? kSecureServiceURL : kServiceURL);
  URLFetcher* url_fetcher =
      URLFetcher::Create(request->routing_id /* used in tests */,
                         url, URLFetcher::POST, this);
  url_fetcher->set_upload_data("application/x-www-form-urlencoded",
                               request->query);
  url_fetcher->set_request_context(Profile::GetDefaultRequestContext());
  url_fetcher->Start();

  // renderer_request_infos_ will now own the RendererRequestInfoList.
  renderer_request_infos_[url_fetcher] =
      request->renderer_request_info.release();

  // Remove the request from the translation request map.
  TranslationRequestMap& translation_request_map =
      request->secure ? pending_secure_translation_requests_ :
                        pending_translation_requests_;
  TranslationRequestMap::iterator iter =
      translation_request_map.find(request->routing_id);
  DCHECK(iter != translation_request_map.end());
  translation_request_map.erase(iter);
  delete request;
}

void TranslationService::SendResponseToRenderer(
    const URLFetcher* const_url_fetcher, int error_code,
    const TextChunksList& text_chunks_list) {
  scoped_ptr<const URLFetcher> url_fetcher(const_url_fetcher);
  RendererRequestInfoMap::iterator iter =
      renderer_request_infos_.find(url_fetcher.get());
  DCHECK(iter != renderer_request_infos_.end());
  scoped_ptr<RendererRequestInfoList> request_info_list(iter->second);
  DCHECK(error_code != 0 ||
         request_info_list->size() == text_chunks_list.size());
  for (size_t i = 0; i < request_info_list->size(); ++i) {
    RendererRequestInfo& request_info = request_info_list->at(i);
    message_sender_->Send(
        new ViewMsg_TranslateTextReponse(request_info.routing_id,
                                         request_info.work_id,
                                         error_code,
                                         error_code ? TextChunks() :
                                                      text_chunks_list[i]));
  }
  renderer_request_infos_.erase(iter);
}

void TranslationService::TranslationFailed(const URLFetcher* url_fetcher) {
  SendResponseToRenderer(url_fetcher, 1, TranslationService::TextChunksList());
}

// static
string16 TranslationService::MergeTextChunks(const TextChunks& text_chunks) {
  // If there is only 1 chunk, we don't need an anchor tag as there is no order
  // to preserve.
  if (text_chunks.size() == 1U)
    return text_chunks[0];

  string16 str;
  for (size_t i = 0; i < text_chunks.size(); ++i) {
    str.append(ASCIIToUTF16("<a _CR_TR_ id='"));
    str.append(IntToString16(i));
    str.append(ASCIIToUTF16("'>"));
    str.append(text_chunks[i]);
    str.append(ASCIIToUTF16("</a>"));
  }
  return str;
}

// static
void TranslationService::SplitTextChunks(const string16& translated_text,
                                         TextChunks* text_chunks) {
  const string16 kOpenTag = ASCIIToUTF16("<a _CR_TR_ ");
  const string16 kCloseTag = ASCIIToUTF16("</a>");
  const size_t open_tag_len = kOpenTag.size();

  size_t start_index = translated_text.find(kOpenTag);
  if (start_index == std::string::npos) {
    // No magic anchor tag, it was a single chunk.
    text_chunks->push_back(translated_text);
    return;
  }

  // The server might send us some HTML with duplicated and unbalanced tags.
  // We separate from the open tag to the next open tag located after at least
  // one close tag.
  while (start_index != std::string::npos) {
    size_t stop_index =
        translated_text.find(kCloseTag, start_index + open_tag_len);
    string16 chunk;
    if (stop_index == std::string::npos) {
      // No close tag.  Just report as one chunk.
      chunk = translated_text;
      start_index = std::string::npos;  // So we break on next iteration.
    } else {
      // Now find the next open tag after this close tag.
      stop_index = translated_text.find(kOpenTag, stop_index);
      if (stop_index != std::string::npos) {
        chunk = translated_text.substr(start_index, stop_index - start_index);
        start_index = stop_index;
      } else {
        chunk = translated_text.substr(start_index);
        start_index = std::string::npos;  // So we break on next iteration.
      }
    }
    chunk = RemoveTag(chunk);
    // The translation server leaves some ampersand character in the
    // translation.
    chunk = UnescapeForHTML(chunk);
    text_chunks->push_back(RemoveTag(chunk));
  }
}

// static
string16 TranslationService::RemoveTag(const string16& text) {
  // Remove any anchor tags, knowing they could be extra/unbalanced tags.
  const string16 kStartTag(ASCIIToUTF16("<a "));
  const string16 kEndTag(ASCIIToUTF16("</a>"));
  const string16 kGreaterThan(ASCIIToUTF16(">"));
  const string16 kLessThan(ASCIIToUTF16("<"));

  string16 result;
  size_t start_index = text.find(kStartTag);
  if (start_index == std::string::npos) {
    result = text;
  } else {
    bool first_iter = true;
    while (true) {
      size_t stop_index = text.find(kGreaterThan, start_index);
      size_t next_tag_index = text.find(kLessThan, start_index + 1);
      // Ignore unclosed <a tag. (Ignore subsequent closing tags, they'll be
      // removed in the next loop.)
      if (stop_index == std::string::npos ||
          (next_tag_index != std::string::npos &&
           stop_index > next_tag_index)) {
        result.append(text.substr(start_index));
        break;
      }
      if (start_index > 0 && first_iter)
        result = text.substr(0, start_index);
      start_index = text.find(kStartTag, start_index + 1);
      if (start_index == std::string::npos) {
        result += text.substr(stop_index + 1);
        break;
      }
      result += text.substr(stop_index + 1, start_index - stop_index - 1);
      first_iter = false;
    }
  }

  // Now remove </a> tags.
  ReplaceSubstringsAfterOffset(&result, 0,
                               ASCIIToUTF16("</a>"), ASCIIToUTF16(""));
  return result;
}

// static
void TranslationService::AddTextToRequestString(std::string* request,
                                                const std::string& text,
                                                const std::string& source_lang,
                                                const std::string& target_lang,
                                                bool secure) {
  if (request->empty()) {
    // First request, add required parameters.
    request->append(kVersionParam);
    request->append("=1.0&");
    request->append(kClientParam);
    request->append("=cr&");  // cr = Chrome.
    request->append(kFormatParam);
    request->append("=html&");
    request->append(kLangPairParam);
    request->append("=");
    request->append(source_lang);
    request->append("%7C");  // | URL encoded.
    request->append(target_lang);
    if (secure) {
      request->append("&");
      request->append(kSSLParam);
      request->append("=1");
    }
  }
  request->append("&");
  request->append(kTextParam);
  request->append("=");
  request->append(text);
}
