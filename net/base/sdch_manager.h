// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides global database of differential decompression dictionaries for the
// SDCH filter (processes sdch enconded content).

// Exactly one instance of SdchManager is built, and all references are made
// into that collection.
//
// The SdchManager maintains a collection of memory resident dictionaries. It
// can find a dictionary (based on a server specification of a hash), store a
// dictionary, and make judgements about what URLs can use, set, etc. a
// dictionary.

// These dictionaries are acquired over the net, and include a header
// (containing metadata) as well as a VCDIFF dictionary (for use by a VCDIFF
// module) to decompress data.

#ifndef NET_BASE_SDCH_MANAGER_H_
#define NET_BASE_SDCH_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/sdch_problem_codes.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace net {

//------------------------------------------------------------------------------
// Create a public interface to help us load SDCH dictionaries.
// The SdchManager class allows registration to support this interface.
// A browser may register a fetcher that is used by the dictionary managers to
// get data from a specified URL. This allows us to use very high level browser
// functionality in this base (when the functionality can be provided).
class NET_EXPORT SdchFetcher {
 public:
  class NET_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

    // Called whenever the SdchFetcher has successfully retrieved a
    // dictionary.  |dictionary_text| contains the body of the dictionary
    // retrieved from |dictionary_url|.
    virtual SdchProblemCode AddSdchDictionary(
        const std::string& dictionary_text,
        const GURL& dictionary_url) = 0;
  };

  SdchFetcher() {}
  virtual ~SdchFetcher() {}

  // The Schedule() method is called when there is a need to get a dictionary
  // from a server. The callee is responsible for getting that dictionary_text,
  // and then calling back to AddSdchDictionary() in the Delegate instance.
  virtual bool Schedule(const GURL& dictionary_url) = 0;

  // The Cancel() method is called to cancel all pending dictionary fetches.
  // This is used for implementation of ClearData() below.
  virtual void Cancel() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SdchFetcher);
};

//------------------------------------------------------------------------------

class NET_EXPORT SdchManager
    : public SdchFetcher::Delegate,
      public NON_EXPORTED_BASE(base::NonThreadSafe) {
 public:
  // Use the following static limits to block DOS attacks until we implement
  // a cached dictionary evicition strategy.
  static const size_t kMaxDictionarySize;
  static const size_t kMaxDictionaryCount;

  // There is one instance of |Dictionary| for each memory-cached SDCH
  // dictionary.
  class NET_EXPORT_PRIVATE Dictionary : public base::RefCounted<Dictionary> {
   public:
    // Sdch filters can get our text to use in decoding compressed data.
    const std::string& text() const { return text_; }

   private:
    friend class base::RefCounted<Dictionary>;
    friend class SdchManager;  // Only manager can construct an instance.
    FRIEND_TEST_ALL_PREFIXES(SdchManagerTest, PathMatch);

    // Construct a vc-diff usable dictionary from the dictionary_text starting
    // at the given offset. The supplied client_hash should be used to
    // advertise the dictionary's availability relative to the suppplied URL.
    Dictionary(const std::string& dictionary_text,
               size_t offset,
               const std::string& client_hash,
               const GURL& url,
               const std::string& domain,
               const std::string& path,
               const base::Time& expiration,
               const std::set<int>& ports);
    virtual ~Dictionary();

    const GURL& url() const { return url_; }
    const std::string& client_hash() const { return client_hash_; }
    const std::string& domain() const { return domain_; }
    const std::string& path() const { return path_; }
    const base::Time& expiration() const { return expiration_; }
    const std::set<int>& ports() const { return ports_; }

    // Security method to check if we can advertise this dictionary for use
    // if the |target_url| returns SDCH compressed data.
    SdchProblemCode CanAdvertise(const GURL& target_url) const;

    // Security methods to check if we can establish a new dictionary with the
    // given data, that arrived in response to get of dictionary_url.
    static SdchProblemCode CanSet(const std::string& domain,
                                  const std::string& path,
                                  const std::set<int>& ports,
                                  const GURL& dictionary_url);

    // Security method to check if we can use a dictionary to decompress a
    // target that arrived with a reference to this dictionary.
    SdchProblemCode CanUse(const GURL& referring_url) const;

    // Compare paths to see if they "match" for dictionary use.
    static bool PathMatch(const std::string& path,
                          const std::string& restriction);

    // Compare domains to see if the "match" for dictionary use.
    static bool DomainMatch(const GURL& url, const std::string& restriction);

    // The actual text of the dictionary.
    std::string text_;

    // Part of the hash of text_ that the client uses to advertise the fact that
    // it has a specific dictionary pre-cached.
    std::string client_hash_;

    // The GURL that arrived with the text_ in a URL request to specify where
    // this dictionary may be used.
    const GURL url_;

    // Metadate "headers" in before dictionary text contained the following:
    // Each dictionary payload consists of several headers, followed by the text
    // of the dictionary. The following are the known headers.
    const std::string domain_;
    const std::string path_;
    const base::Time expiration_;  // Implied by max-age.
    const std::set<int> ports_;

    DISALLOW_COPY_AND_ASSIGN(Dictionary);
  };

  SdchManager();
  ~SdchManager() override;

  // Clear data (for browser data removal).
  void ClearData();

  // Record stats on various errors.
  static void SdchErrorRecovery(SdchProblemCode problem);

  // Register a fetcher that this class can use to obtain dictionaries.
  void set_sdch_fetcher(scoped_ptr<SdchFetcher> fetcher);

  // Enables or disables SDCH compression.
  static void EnableSdchSupport(bool enabled);

  static bool sdch_enabled() { return g_sdch_enabled_; }

  // Enables or disables SDCH compression over secure connection.
  static void EnableSecureSchemeSupport(bool enabled);

  static bool secure_scheme_supported() { return g_secure_scheme_supported_; }

  // Briefly prevent further advertising of SDCH on this domain (if SDCH is
  // enabled). After enough calls to IsInSupportedDomain() the blacklisting
  // will be removed. Additional blacklists take exponentially more calls
  // to IsInSupportedDomain() before the blacklisting is undone.
  // Used when filter errors are found from a given domain, but it is plausible
  // that the cause is temporary (such as application startup, where cached
  // entries are used, but a dictionary is not yet loaded).
  void BlacklistDomain(const GURL& url, SdchProblemCode blacklist_reason);

  // Used when SEVERE filter errors are found from a given domain, to prevent
  // further use of SDCH on that domain.
  void BlacklistDomainForever(const GURL& url,
                              SdchProblemCode blacklist_reason);

  // Unit test only, this function resets enabling of sdch, and clears the
  // blacklist.
  void ClearBlacklistings();

  // Unit test only, this function resets the blacklisting count for a domain.
  void ClearDomainBlacklisting(const std::string& domain);

  // Unit test only: indicate how many more times a domain will be blacklisted.
  int BlackListDomainCount(const std::string& domain);

  // Unit test only: Indicate what current blacklist increment is for a domain.
  int BlacklistDomainExponential(const std::string& domain);

  // Check to see if SDCH is enabled (globally), and the given URL is in a
  // supported domain (i.e., not blacklisted, and either the specific supported
  // domain, or all domains were assumed supported). If it is blacklist, reduce
  // by 1 the number of times it will be reported as blacklisted.
  SdchProblemCode IsInSupportedDomain(const GURL& url);

  // Schedule the URL fetching to load a dictionary. This will always return
  // before the dictionary is actually loaded and added.
  // After the implied task does completes, the dictionary will have been
  // cached in memory.
  SdchProblemCode FetchDictionary(const GURL& request_url,
                                  const GURL& dictionary_url);

  // Security test function used before initiating a FetchDictionary.
  // Return PROBLEM_CODE_OK if fetch is legal.
  SdchProblemCode CanFetchDictionary(const GURL& referring_url,
                                     const GURL& dictionary_url) const;

  // Find the vcdiff dictionary (the body of the sdch dictionary that appears
  // after the meta-data headers like Domain:...) with the given |server_hash|
  // to use to decompreses data that arrived as SDCH encoded content. Check to
  // be sure the returned |dictionary| can be used for decoding content supplied
  // in response to a request for |referring_url|.
  // Return null in |dictionary| if there is no matching legal dictionary.
  // Returns SDCH_OK if dictionary is not found, SDCH(-over-https) is disabled,
  // or if matching legal dictionary exists. Otherwise returns the
  // corresponding problem code.
  SdchProblemCode GetVcdiffDictionary(const std::string& server_hash,
                                      const GURL& referring_url,
                                      scoped_refptr<Dictionary>* dictionary);

  // Get list of available (pre-cached) dictionaries that we have already loaded
  // into memory. The list is a comma separated list of (client) hashes per
  // the SDCH spec.
  void GetAvailDictionaryList(const GURL& target_url, std::string* list);

  // Construct the pair of hashes for client and server to identify an SDCH
  // dictionary. This is only made public to facilitate unit testing, but is
  // otherwise private
  static void GenerateHash(const std::string& dictionary_text,
                           std::string* client_hash, std::string* server_hash);

  // For Latency testing only, we need to know if we've succeeded in doing a
  // round trip before starting our comparative tests. If ever we encounter
  // problems with SDCH, we opt-out of the test unless/until we perform a
  // complete SDCH decoding.
  bool AllowLatencyExperiment(const GURL& url) const;

  void SetAllowLatencyExperiment(const GURL& url, bool enable);

  base::Value* SdchInfoToValue() const;

  int GetFetchesCountForTesting() const {
    return fetches_count_for_testing_;
  }

  // Implementation of SdchFetcher::Delegate.

  // Add an SDCH dictionary to our list of availible
  // dictionaries. This addition will fail if addition is illegal
  // (data in the dictionary is not acceptable from the
  // dictionary_url; dictionary already added, etc.).
  // Returns SDCH_OK if the addition was successfull, and corresponding error
  // code otherwise.
  SdchProblemCode AddSdchDictionary(const std::string& dictionary_text,
                                    const GURL& dictionary_url) override;

 private:
  struct BlacklistInfo {
    BlacklistInfo() : count(0), exponential_count(0), reason(SDCH_OK) {}

    int count;               // # of times to refuse SDCH advertisement.
    int exponential_count;   // Current exponential backoff ratchet.
    SdchProblemCode reason;  // Why domain was blacklisted.
  };
  typedef std::map<std::string, BlacklistInfo> DomainBlacklistInfo;
  typedef std::set<std::string> ExperimentSet;

  // A map of dictionaries info indexed by the hash that the server provides.
  typedef std::map<std::string, scoped_refptr<Dictionary> > DictionaryMap;

  // Support SDCH compression, by advertising in headers.
  static bool g_sdch_enabled_;

  // Support SDCH compression for HTTPS requests and responses. When supported,
  // HTTPS applicable dictionaries MUST have been acquired securely via HTTPS.
  static bool g_secure_scheme_supported_;

  // A simple implementation of a RFC 3548 "URL safe" base64 encoder.
  static void UrlSafeBase64Encode(const std::string& input,
                                  std::string* output);

  DictionaryMap dictionaries_;

  // An instance that can fetch a dictionary given a URL.
  scoped_ptr<SdchFetcher> fetcher_;

  // List domains where decode failures have required disabling sdch.
  DomainBlacklistInfo blacklisted_domains_;

  // List of hostnames for which a latency experiment is allowed (because a
  // round trip test has recently passed).
  ExperimentSet allow_latency_experiment_;

  int fetches_count_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(SdchManager);
};

}  // namespace net

#endif  // NET_BASE_SDCH_MANAGER_H_
