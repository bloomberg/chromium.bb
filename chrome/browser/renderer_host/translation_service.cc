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

// Mapping from a locale name to a language code name.
// Locale names not included are translated as is.
struct LocaleToCLDLanguage {
  const char* locale_language;  // Language Chrome locale is in.
  const char* cld_language;     // Language the CLD reports.
};
LocaleToCLDLanguage kLocaleToCLDLanguages[] = {
    { "en-GB", "en" },
    { "en-US", "en" },
    { "es-419", "es" },
    { "pt-BR", "pt" },
    { "pt-PT", "pt" },
};

// The list of languages the Google translation server supports.
// For information, here is the list of languages that Chrome can be run into
// but that the translation server does not support:
// am Amharic
// bn Bengali
// gu Gujarati
// kn Kannada
// ml Malayalam
// mr Marathi
// or Oriya
// ta Tamil
// te Telugu
const char* kSupportedLanguages[] = {
    "af",     // Afrikaans
    "sq",     // Albanian
    "ar",     // Arabic
    "be",     // Belarusian
    "bg",     // Bulgarian
    "ca",     // Catalan
    "zh-CN",  // Chinese (Simplified)
    "zh-TW",  // Chinese (Traditional)
    "hr",     // Croatian
    "cs",     // Czech
    "da",     // Danish
    "nl",     // Dutch
    "en",     // English
    "et",     // Estonian
    "fi",     // Finnish
    "fil",    // Filipino
    "fr",     // French
    "gl",     // Galician
    "de",     // German
    "el",     // Greek
    "he",     // Hebrew
    "hi",     // Hindi
    "hu",     // Hungarian
    "is",     // Icelandic
    "id",     // Indonesian
    "it",     // Italian
    "ga",     // Irish
    "ja",     // Japanese
    "ko",     // Korean
    "lv",     // Latvian
    "lt",     // Lithuanian
    "mk",     // Macedonian
    "ms",     // Malay
    "mt",     // Maltese
    "nb",     // Norwegian
    "fa",     // Persian
    "pl",     // Polish
    "pt",     // Portuguese
    "ro",     // Romanian
    "ru",     // Russian
    "sr",     // Serbian
    "sk",     // Slovak
    "sl",     // Slovenian
    "es",     // Spanish
    "sw",     // Swahili
    "sv",     // Swedish
    "th",     // Thai
    "tr",     // Turkish
    "uk",     // Ukrainian
    "vi",     // Vietnamese
    "cy",     // Welsh
    "yi",     // Yiddish
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

// static
// The string is: '&' + kTextParam + '='.
size_t TranslationService::text_param_length_ = 1 + arraysize(kTextParam) + 1;

// static
base::LazyInstance<std::set<std::string> >
    TranslationService::supported_languages_(base::LINKER_INITIALIZED);

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
    : message_sender_(message_sender),
      kCRAnchorTagStart(ASCIIToUTF16("<a _CR_TR_ id='")),
      kAnchorTagStart(ASCIIToUTF16("<a ")),
      kClosingAnchorTag(ASCIIToUTF16("</a>")),
      kQuote(ASCIIToUTF16("'")),
      kGreaterThan(ASCIIToUTF16(">")),
      kLessThan(ASCIIToUTF16("<")),
      kQuoteGreaterThan(ASCIIToUTF16("'>")) {
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
      if (translation_request->query.size() + text.size() +
              text_param_length_ >= kTextRequestMaxSize) {
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
  std::string wrapped_data;
  if (data.size() > 1U && data[0] == '"') {
    wrapped_data.append("[");
    wrapped_data.append(data);
    wrapped_data.append("]");
  }
  scoped_ptr<Value> value(base::JSONReader::Read(
      wrapped_data.empty() ? data : wrapped_data, true));
  if (!value.get()) {
    NOTREACHED() << "Translation server returned invalid JSON response.";
    TranslationFailed(source);
    return;
  }

  // If the request was for a single string, the response is the translated
  // string.
  TextChunksList translated_chunks_list;
  if (value->IsType(Value::TYPE_STRING)) {
    string16 translated_text;
    if (!value->GetAsUTF16(&translated_text)) {
      NOTREACHED();
      TranslationFailed(source);
      return;
    }
    TextChunks translated_text_chunks;
    translated_text_chunks.push_back(translated_text);
    translated_chunks_list.push_back(translated_text_chunks);
  } else {
    if (!value->IsType(Value::TYPE_LIST)) {
      NOTREACHED() << "Translation server returned unexpected JSON response "
          " (not a list).";
      TranslationFailed(source);
      return;
    }
    ListValue* translated_text_list = static_cast<ListValue*>(value.get());
    for (size_t i = 0; i < translated_text_list->GetSize(); ++i) {
      string16 translated_text;
      if (!translated_text_list->GetStringAsUTF16(i, &translated_text)) {
        NOTREACHED() << "Translation server returned unexpected JSON response "
            " (unexpected type in list).";
        TranslationFailed(source);
        return;
      }
      translated_text = UnescapeForHTML(translated_text);
      TranslationService::TextChunks translated_text_chunks;
      TranslationService::SplitIntoTextChunks(translated_text,
                                              &translated_text_chunks);
      translated_chunks_list.push_back(translated_text_chunks);
    }
  }

  // We have successfully extracted all the translated text chunks, send them to
  // the renderer.
  SendResponseToRenderer(source, 0, translated_chunks_list);
}

// static
bool TranslationService::IsTranslationEnabled() {
  return GURL(kServiceURL).host() != "disabled";
}

// static
void TranslationService::GetSupportedLanguages(
    std::vector<std::string>* languages) {
  DCHECK(languages && languages->empty());
  for (size_t i = 0; i < arraysize(kSupportedLanguages); ++i)
    languages->push_back(kSupportedLanguages[i]);
}

// static
std::string TranslationService::GetLanguageCode(
    const std::string& chrome_locale) {
  for (size_t i = 0; i < arraysize(kLocaleToCLDLanguages); ++i) {
    if (chrome_locale == kLocaleToCLDLanguages[i].locale_language)
      return kLocaleToCLDLanguages[i].cld_language;
  }
  return chrome_locale;
}

// static
bool TranslationService::IsSupportedLanguage(const std::string& page_language) {
  if (supported_languages_.Pointer()->empty()) {
    for (size_t i = 0; i < arraysize(kSupportedLanguages); ++i)
      supported_languages_.Pointer()->insert(kSupportedLanguages[i]);
  }
  return supported_languages_.Pointer()->find(page_language) !=
      supported_languages_.Pointer()->end();
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

string16 TranslationService::MergeTextChunks(const TextChunks& text_chunks) {
  // If there is only 1 chunk, we don't need an anchor tag as there is no order
  // to preserve.
  if (text_chunks.size() == 1U)
    return text_chunks[0];

  string16 str;
  for (size_t i = 0; i < text_chunks.size(); ++i) {
    str.append(kCRAnchorTagStart);
    str.append(IntToString16(i));
    str.append(kQuoteGreaterThan);
    str.append(text_chunks[i]);
    str.append(kClosingAnchorTag);
  }
  return str;
}

bool TranslationService::FindOpenTagIndex(const string16& text,
                                          size_t start_index,
                                          size_t* tag_start_index,
                                          size_t* tag_end_index,
                                          int* id) {
  DCHECK(tag_start_index && tag_end_index && id);
  size_t text_length = text.length();
  if (start_index >= text_length)
    return false;

  *tag_start_index = text.find(kCRAnchorTagStart, start_index);
  if (*tag_start_index == std::string::npos)
    return false;

  size_t quote_index = *tag_start_index + kCRAnchorTagStart.length();
  size_t close_quote_index = text.find(kQuote, quote_index);
  if (close_quote_index == std::string::npos) {
    NOTREACHED();
    return false;  // Not a valid anchor tag.
  }

  string16 id_str = text.substr(quote_index, close_quote_index - quote_index);
  // Get the id.
  if (!StringToInt(id_str, id)) {
    NOTREACHED();
    return false;  // Not a valid id, give up.
  }

  *tag_end_index = text.find(kGreaterThan, close_quote_index);
  if (*tag_end_index == std::string::npos || *tag_end_index >= text_length)
    return false;
  return true;
}

void TranslationService::SplitIntoTextChunks(const string16& translated_text,
                                             TextChunks* text_chunks) {
  int id = -1;
  size_t tag_start_index = 0;
  size_t tag_end_index = 0;
  if (!FindOpenTagIndex(translated_text, 0, &tag_start_index, &tag_end_index,
                        &id)) {
    // No magic anchor tag, it was a single chunk.
    text_chunks->push_back(translated_text);
    return;
  }

  // The server might send us some HTML with duplicated and unbalanced tags.
  // We separate from one tag begining to the next, and merge tags with
  // duplicate IDs.
  std::set<int> parsed_tags;
  string16 chunk;
  while (tag_start_index != std::string::npos) {
    int next_id = -1;
    size_t previous_tag_end_index = tag_end_index;
    if (!FindOpenTagIndex(translated_text, tag_end_index,
                          &tag_start_index, &tag_end_index, &next_id)) {
      // Last tag. Just report as one chunk.
      chunk = translated_text.substr(previous_tag_end_index + 1);
      tag_start_index = std::string::npos;  // So we break on next iteration.
    } else {
      // Extract the text for this tag.
      DCHECK(tag_start_index > previous_tag_end_index);
      chunk =
          translated_text.substr(previous_tag_end_index + 1,
                                 tag_start_index - previous_tag_end_index - 1);
    }
    chunk = RemoveTag(chunk);
    // The translation server leaves some ampersand character in the
    // translation.
    chunk = UnescapeForHTML(chunk);
    if (parsed_tags.count(id) > 0) {
      // We have already seen this tag, add it to the previous text-chunk.
      text_chunks->back().append(chunk);
    } else {
      text_chunks->push_back(chunk);
      parsed_tags.insert(id);
    }
    id = next_id;
  }
}

string16 TranslationService::RemoveTag(const string16& text) {
  // Remove any anchor tags, knowing they could be extra/unbalanced tags.
  string16 result;
  size_t start_index = text.find(kAnchorTagStart);
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
      start_index = text.find(kAnchorTagStart, start_index + 1);
      if (start_index == std::string::npos) {
        result += text.substr(stop_index + 1);
        break;
      }
      result += text.substr(stop_index + 1, start_index - stop_index - 1);
      first_iter = false;
    }
  }

  // Now remove </a> tags.
  ReplaceSubstringsAfterOffset(&result, 0, kClosingAnchorTag, EmptyString16());
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
  // IMPORTANT NOTE: if you make any change below, make sure to reflect them in
  //                 text_param_length_ in TranslationService constructor.
  request->append("&");
  request->append(kTextParam);
  request->append("=");
  request->append(text);
}
