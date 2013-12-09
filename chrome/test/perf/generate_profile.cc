// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/perf/generate_profile.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/codec/jpeg_codec.h"

using base::Time;
using content::BrowserThread;

namespace {

// Probabilities of different word lengths, as measured from Darin's profile.
//   kWordLengthProbabilities[n-1] = P(word of length n)
const float kWordLengthProbabilities[] = { 0.069f, 0.132f, 0.199f,
  0.137f, 0.088f, 0.115f, 0.081f, 0.055f, 0.034f, 0.021f, 0.019f, 0.018f,
  0.007f, 0.007f, 0.005f, 0.004f, 0.003f, 0.003f, 0.003f };

// Return a float uniformly in [0,1].
// Useful for making probabilistic decisions.
inline float RandomFloat() {
  return rand() / static_cast<float>(RAND_MAX);
}

// Return an integer uniformly in [min,max).
inline int RandomInt(int min, int max) {
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

  scoped_refptr<base::RefCountedMemory> google_bitmap(
      new base::RefCountedStaticMemory(kGoogleThumbnail,
                                       sizeof(kGoogleThumbnail)));
  scoped_refptr<base::RefCountedMemory> weewar_bitmap(
      new base::RefCountedStaticMemory(kWeewarThumbnail,
                                       sizeof(kWeewarThumbnail)));

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
    if (types & TOP_SITES && top_sites) {
      top_sites->SetPageThumbnailToJPEGBytes(
          url,
          (RandomInt(0, 2) == 0) ? google_bitmap.get() : weewar_bitmap.get(),
          score);
    }

    previous_url = url;

    if (revisit_urls.empty() || RandomFloat() < kRevisitableURLProbability)
      revisit_urls.push_back(url);
  }
}

}  // namespace

bool GenerateProfile(GenerateProfileTypes types,
                     int url_count,
                     const base::FilePath& dst_dir) {
  if (!base::CreateDirectory(dst_dir)) {
    PLOG(ERROR) << "Unable to create directory " << dst_dir.value().c_str();
    return false;
  }

  // We want this profile to be as deterministic as possible, so seed the
  // random number generator with the number of urls we're generating.
  srand(static_cast<unsigned int>(url_count));

  printf("Creating profiles for testing...\n");

  TestingBrowserProcessInitializer initialize_browser_process;
  base::MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread db_thread(BrowserThread::DB, &message_loop);
  TestingProfile profile;
  if (!profile.CreateHistoryService(false, false)) {
      PLOG(ERROR) << "Creating history service failed";
      return false;
  }
  if (types & TOP_SITES) {
    profile.CreateTopSites();
    profile.BlockUntilTopSitesLoaded();
  }

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

  profile.DestroyTopSites();
  profile.DestroyHistoryService();

  message_loop.RunUntilIdle();

  base::FileEnumerator file_iterator(profile.GetPath(), false,
                                     base::FileEnumerator::FILES);
  base::FilePath path = file_iterator.Next();
  while (!path.empty()) {
    base::FilePath dst_file = dst_dir.Append(path.BaseName());
    base::DeleteFile(dst_file, false);
    if (!base::CopyFile(path, dst_file)) {
      PLOG(ERROR) << "Copying file failed";
      return false;
    }
    path = file_iterator.Next();
  }

  printf("Finished creating profiles for testing.\n");

  // Restore the random seed.
  srand(static_cast<unsigned int>(Time::Now().ToInternalValue()));

  return true;
}
