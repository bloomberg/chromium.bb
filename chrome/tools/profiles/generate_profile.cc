// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This program generates a user profile and history by randomly generating
// data and feeding it to the history service.

#include "chrome/tools/profiles/thumbnail-inl.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/browser_thread_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/codec/jpeg_codec.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif

using base::Time;
using content::BrowserThread;

// Addition types data can be generated for. By default only urls/visits are
// added.
enum Types {
  TOP_SITES = 1 << 0,
  FULL_TEXT = 1 << 1
};

// RAII for initializing and shutting down the TestBrowserProcess
class InitBrowserProcess {
 public:
  InitBrowserProcess() {
    DCHECK(!g_browser_process);
    g_browser_process = new TestingBrowserProcess;
  }

  ~InitBrowserProcess() {
    DCHECK(g_browser_process);
    delete g_browser_process;
    g_browser_process = NULL;
  }
};

// Probabilities of different word lengths, as measured from Darin's profile.
//   kWordLengthProbabilities[n-1] = P(word of length n)
const float kWordLengthProbabilities[] = { 0.069f, 0.132f, 0.199f,
  0.137f, 0.088f, 0.115f, 0.081f, 0.055f, 0.034f, 0.021f, 0.019f, 0.018f,
  0.007f, 0.007f, 0.005f, 0.004f, 0.003f, 0.003f, 0.003f };

// Return a float uniformly in [0,1].
// Useful for making probabilistic decisions.
float RandomFloat() {
  return rand() / static_cast<float>(RAND_MAX);
}

// Return an integer uniformly in [min,max).
int RandomInt(int min, int max) {
  return min + (rand() % (max-min));
}

// Return a string of |count| lowercase random characters.
string16 RandomChars(int count) {
  string16 str;
  for (int i = 0; i < count; ++i)
    str += L'a' + rand() % 26;
  return str;
}

string16 RandomWord() {
  // TODO(evanm): should we instead use the markov chain based
  // version of this that I already wrote?

  // Sample a word length from kWordLengthProbabilities.
  float sample = RandomFloat();
  size_t i;
  for (i = 0; i < arraysize(kWordLengthProbabilities); ++i) {
    sample -= kWordLengthProbabilities[i];
    if (sample < 0) break;
  }
  const int word_length = i + 1;
  return RandomChars(word_length);
}

// Return a string of |count| random words.
string16 RandomWords(int count) {
  string16 str;
  for (int i = 0; i < count; ++i) {
    if (!str.empty())
      str += L' ';
    str += RandomWord();
  }
  return str;
}

// Return a random URL-looking string.
GURL ConstructRandomURL() {
  return GURL(ASCIIToUTF16("http://") + RandomChars(3) + ASCIIToUTF16(".com/") +
      RandomChars(RandomInt(5, 20)));
}

// Return a random page title-looking string.
string16 ConstructRandomTitle() {
  return RandomWords(RandomInt(3, 15));
}

// Return a random string that could function as page contents.
string16 ConstructRandomPage() {
  return RandomWords(RandomInt(10, 4000));
}

// Insert a batch of |batch_size| URLs, starting at pageid |page_id|.
void InsertURLBatch(Profile* profile,
                    int page_id,
                    int batch_size,
                    int types) {
  HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile, Profile::EXPLICIT_ACCESS);

  // Probability of following a link on the current "page"
  // (vs randomly jumping to a new page).
  const float kFollowLinkProbability = 0.85f;
  // Probability of visiting a page we've visited before.
  const float kRevisitLinkProbability = 0.1f;
  // Probability of a URL being "good enough" to revisit.
  const float kRevisitableURLProbability = 0.05f;
  // Probability of a URL being the end of a redirect chain.
  const float kRedirectProbability = 0.05f;

  // A list of URLs that we sometimes revisit.
  std::vector<GURL> revisit_urls;

  // Scoping value for page IDs (required by the history service).
  void* id_scope = reinterpret_cast<void*>(1);

  scoped_ptr<SkBitmap> google_bitmap(
      gfx::JPEGCodec::Decode(kGoogleThumbnail, sizeof(kGoogleThumbnail)));
  scoped_ptr<SkBitmap> weewar_bitmap(
      gfx::JPEGCodec::Decode(kWeewarThumbnail, sizeof(kWeewarThumbnail)));

  printf("Inserting %d URLs...\n", batch_size);
  GURL previous_url;
  content::PageTransition transition = content::PAGE_TRANSITION_TYPED;
  const int end_page_id = page_id + batch_size;
  history::TopSites* top_sites = profile->GetTopSites();
  for (; page_id < end_page_id; ++page_id) {
    // Randomly decide whether this new URL simulates following a link or
    // whether it's a jump to a new URL.
    if (!previous_url.is_empty() && RandomFloat() < kFollowLinkProbability) {
      transition = content::PAGE_TRANSITION_LINK;
    } else {
      previous_url = GURL();
      transition = content::PAGE_TRANSITION_TYPED;
    }

    // Pick a URL, either newly at random or from our list of previously
    // visited URLs.
    GURL url;
    if (!revisit_urls.empty() && RandomFloat() < kRevisitLinkProbability) {
      // Draw a URL from revisit_urls at random.
      url = revisit_urls[RandomInt(0, static_cast<int>(revisit_urls.size()))];
    } else {
      url = ConstructRandomURL();
    }

    // Randomly construct a redirect chain.
    history::RedirectList redirects;
    if (RandomFloat() < kRedirectProbability) {
      const int redir_count = RandomInt(1, 4);
      for (int i = 0; i < redir_count; ++i)
        redirects.push_back(ConstructRandomURL());
      redirects.push_back(url);
    }

    // Add all of this information to the history service.
    history_service->AddPage(url, base::Time::Now(),
                             id_scope, page_id,
                             previous_url, redirects,
                             transition, history::SOURCE_BROWSED, true);
    ThumbnailScore score(0.75, false, false);
    history_service->SetPageTitle(url, ConstructRandomTitle());
    if (types & FULL_TEXT)
      history_service->SetPageContents(url, ConstructRandomPage());
    if (types & TOP_SITES && top_sites) {
      const SkBitmap& bitmap = (RandomInt(0, 2) == 0) ? *google_bitmap :
                                                        *weewar_bitmap;
      gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);
      top_sites->SetPageThumbnail(url, image, score);
    }

    previous_url = url;

    if (revisit_urls.empty() || RandomFloat() < kRevisitableURLProbability)
      revisit_urls.push_back(url);
  }
}

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  base::EnableTerminationOnHeapCorruption();
  base::AtExitManager exit_manager;
  CommandLine* cl = CommandLine::ForCurrentProcess();

  int types = 0;
  if (cl->HasSwitch("top-sites"))
    types |= TOP_SITES;
  if (cl->HasSwitch("full-text"))
    types |= FULL_TEXT;

  // We require two arguments: urlcount and profiledir.
  const CommandLine::StringVector& args = cl->GetArgs();
  if (args.size() < 2) {
    printf("usage: %s [--top-sites] [--full-text] <urlcount> "
           "<profiledir>\n", argv[0]);
    printf("\n  --top-sites Generate thumbnails\n");
    printf("\n  --full-text Generate full text index\n");
    return -1;
  }

  int url_count = 0;
  base::StringToInt(args[0], &url_count);
  FilePath dst_dir(args[1]);
  if (!dst_dir.IsAbsolute()) {
    FilePath current_dir;
    file_util::GetCurrentDirectory(&current_dir);
    dst_dir = current_dir.Append(dst_dir);
  }
  if (!file_util::CreateDirectory(dst_dir)) {
    PLOG(ERROR) << "Unable to create directory " << dst_dir.value().c_str();
  }

  icu_util::Initialize();
  // Copied from base/test/test_suite.cc.
#if defined(TOOLKIT_GTK)
  gtk_init_check(&argc, &argv);
#endif

  InitBrowserProcess initialize_browser_process;
  chrome::RegisterPathProvider();
  ui::RegisterPathProvider();
  MessageLoopForUI message_loop;
  content::BrowserThreadImpl ui_thread(BrowserThread::UI, &message_loop);
  content::BrowserThreadImpl db_thread(BrowserThread::DB, &message_loop);
  ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);
  TestingProfile profile;
  profile.CreateHistoryService(false, false);
  if (types & TOP_SITES) {
    profile.CreateTopSites();
    profile.BlockUntilTopSitesLoaded();
  }

  srand(static_cast<unsigned int>(Time::Now().ToInternalValue()));

  // The maximum number of URLs to insert into history in one batch.
  const int kBatchSize = 2000;
  int page_id = 0;
  while (page_id < url_count) {
    const int batch_size = std::min(kBatchSize, url_count - page_id);
    InsertURLBatch(&profile, page_id, batch_size, types);
    // Run all pending messages to give TopSites a chance to catch up.
    message_loop.RunUntilIdle();
    page_id += batch_size;
  }

  printf("Writing to disk\n");

  profile.DestroyTopSites();
  profile.DestroyHistoryService();

  message_loop.RunUntilIdle();

  file_util::FileEnumerator file_iterator(profile.GetPath(), false,
      file_util::FileEnumerator::FILES);
  FilePath path = file_iterator.Next();
  while (!path.empty()) {
    FilePath dst_file = dst_dir.Append(path.BaseName());
    file_util::Delete(dst_file, false);
    printf("Copying file %" PRFilePath " to "
           "%" PRFilePath "\n", path.value().c_str(),
           dst_file.value().c_str());
    if (!file_util::CopyFile(path, dst_file)) {
      PLOG(ERROR) << "Copying file failed";
      return -1;
    }
    path = file_iterator.Next();
  }

  return 0;
}
