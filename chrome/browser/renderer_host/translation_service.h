// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_TRANSLATION_SERVICE_H_
#define CHROME_BROWSER_RENDERER_HOST_TRANSLATION_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/net/url_fetcher.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class DictionaryValue;
class TranslationServiceTest;
class TranslateURLFetcherDelegate;
class URLFetcher;

// The TranslationService class is used to translate text.
// There is one TranslationService is per renderer process.
// It receives requests to translate text from the different render views of the
// render process, provided in lists (text chunks), where the words should be
// translated without changing the chunks order.
// It groups multiple such requests and sends them for translation to the Google
// translation server.  When it receives the response, it dispatches it to the
// appropriate render view.

class TranslationService : public URLFetcher::Delegate {
 public:
  explicit TranslationService(IPC::Message::Sender* message_sender);
  virtual ~TranslationService();

  // Sends the specified text for translation, from |source_language| to
  // |target_language|.  If |secure| is true, a secure connection is used when
  // sending the text to the external translation server.
  // When the translation results have been received, it sends a
  // ViewMsg_TranslateTextReponse message on the renderer at |routing_id|.
  void Translate(int routing_id,
                 int page_id,
                 int work_id,
                 const std::vector<string16>& text_chunks,
                 const std::string& source_language,
                 const std::string& target_language,
                 bool secure);

  // Sends the pending translation request for the specified renderer to the
  // translation server.
  void SendTranslationRequestForRenderer(int renderer_id, bool secure);

  // URLFetcher::Delegate implementation.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // Returns true if a page in the language |page_language| (as reported by the
  // CLD) should be translated when Chrome is using |chrome_language|. Note that
  // this returns false for similar languages, for example it returns false when
  // given the values 'en' and 'en-US'.
  static bool ShouldTranslatePage(const std::string& page_language,
                                  const std::string& chrome_language);

  // Returns true if the TranslationService is enabled.
  static bool IsTranslationEnabled();

  // Fills |languages| with the list of languages that the translate server can
  // translate to and from.
  static void GetSupportedLanguages(std::vector<std::string>* languages);

 protected:
  // The amount of time in ms after which a pending request is sent if no other
  // translation request has been received.
  // Overriden in tests.
  virtual int GetSendRequestDelay() const;

 private:
  friend class TranslationServiceTest;
  friend class TranslateURLFetcherDelegate;
  FRIEND_TEST(TranslationServiceTest, MergeTestChunks);
  FRIEND_TEST(TranslationServiceTest, SplitTestChunks);
  FRIEND_TEST(TranslationServiceTest, RemoveTag);

  struct TranslationRequest;

  // The information necessary to return the translated text to the renderer.
  struct RendererRequestInfo {
    RendererRequestInfo() : routing_id(0), work_id(0) {}
    RendererRequestInfo(int routing_id, int work_id)
        : routing_id(routing_id),
          work_id(work_id) {
    }
    int routing_id;
    int work_id;
  };

  typedef std::vector<RendererRequestInfo> RendererRequestInfoList;

  typedef std::vector<string16> TextChunks;
  typedef std::vector<TextChunks> TextChunksList;
  // Maps from a RenderView routing id to the pending request for the
  // translation server.
  typedef std::map<int, TranslationRequest*> TranslationRequestMap;

  typedef std::map<const URLFetcher*, RendererRequestInfoList*>
      RendererRequestInfoMap;

  // Sends the passed request to the translations server.
  // Warning the request is deleted when this call returns.
  void SendRequestToTranslationServer(TranslationRequest* request);

  // Called by the URLFetcherDelegate when the translation associated with
  // |url_fetcher| has been performed.  Sends the appropriate message back to
  // the renderer and deletes the URLFetcher.
  void SendResponseToRenderer(const URLFetcher* url_fetcher,
                              int error_code,
                              const TextChunksList& text_chunks_list);

  // Notifies the renderer that we failed to translate the request associated
  // with |url_fetcher|.
  void TranslationFailed(const URLFetcher* source);

  // Merges all text chunks to be translated into a single string that can be
  // sent to the translate server, surrounding each chunk with an anchor tag
  // to preserve chunk order in the translated version.
  static string16 MergeTextChunks(const TextChunks& text_chunks);

  // Splits the translated text into its original text chunks, removing the
  // anchor tags wrapper that were added to preserve order.
  static void SplitTextChunks(const string16& translated_text,
                              TextChunks* text_chunks);

  // Removes the HTML anchor tag surrounding |text| and returns the resulting
  // string.
  static string16 RemoveTag(const string16& text);

  // Adds |text| to the string request in/out param |request|.  If |request| is
  // empty, then the source, target language as well as the secure parameters
  // are also added.
  static void AddTextToRequestString(std::string* request,
                                     const std::string& text,
                                     const std::string& source_language,
                                     const std::string& target_language,
                                     bool secure);

  // The channel used to communicate with the renderer.
  IPC::Message::Sender* message_sender_;

  // Map used to retrieve the context of requests when the URLFetcher notifies
  // that it got a response.
  RendererRequestInfoMap renderer_request_infos_;

  TranslationRequestMap pending_translation_requests_;
  TranslationRequestMap pending_secure_translation_requests_;

  DISALLOW_COPY_AND_ASSIGN(TranslationService);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_TRANSLATION_SERVICE_H_
