// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_USER_SCRIPT_H_
#define CHROME_COMMON_EXTENSIONS_USER_SCRIPT_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/string_piece.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "googleurl/src/gurl.h"

class Pickle;
class PickleIterator;

namespace extensions {

// Represents a user script, either a standalone one, or one that is part of an
// extension.
class UserScript {
 public:
  // The file extension for standalone user scripts.
  static const char kFileExtension[];

  // Check if a URL should be treated as a user script and converted to an
  // extension.
  static bool IsURLUserScript(const GURL& url, const std::string& mime_type);

  // Get the valid user script schemes for the current process. If
  // canExecuteScriptEverywhere is true, this will return ALL_SCHEMES.
  static int ValidUserScriptSchemes(bool canExecuteScriptEverywhere = false);

  // Locations that user scripts can be run inside the document.
  enum RunLocation {
    UNDEFINED,
    DOCUMENT_START,  // After the documentElemnet is created, but before
                     // anything else happens.
    DOCUMENT_END,  // After the entire document is parsed. Same as
                   // DOMContentLoaded.
    DOCUMENT_IDLE,  // Sometime after DOMContentLoaded, as soon as the document
                    // is "idle". Currently this uses the simple heuristic of:
                    // min(DOM_CONTENT_LOADED + TIMEOUT, ONLOAD), but no
                    // particular injection point is guaranteed.
    RUN_LOCATION_LAST  // Leave this as the last item.
  };

  // Holds actual script file info.
  class File {
   public:
    File(const base::FilePath& extension_root,
         const base::FilePath& relative_path,
         const GURL& url);
    File();
    ~File();

    const base::FilePath& extension_root() const { return extension_root_; }
    const base::FilePath& relative_path() const { return relative_path_; }

    const GURL& url() const { return url_; }
    void set_url(const GURL& url) { url_ = url; }

    // If external_content_ is set returns it as content otherwise it returns
    // content_
    const base::StringPiece GetContent() const {
      if (external_content_.data())
        return external_content_;
      else
        return content_;
    }
    void set_external_content(const base::StringPiece& content) {
      external_content_ = content;
    }
    void set_content(const base::StringPiece& content) {
      content_.assign(content.begin(), content.end());
    }

    // Serialization support. The content and FilePath members will not be
    // serialized!
    void Pickle(::Pickle* pickle) const;
    void Unpickle(const ::Pickle& pickle, PickleIterator* iter);

   private:
    // Where the script file lives on the disk. We keep the path split so that
    // it can be localized at will.
    base::FilePath extension_root_;
    base::FilePath relative_path_;

    // The url to this scipt file.
    GURL url_;

    // The script content. It can be set to either loaded_content_ or
    // externally allocated string.
    base::StringPiece external_content_;

    // Set when the content is loaded by LoadContent
    std::string content_;
  };

  typedef std::vector<File> FileList;

  // Constructor. Default the run location to document end, which is like
  // Greasemonkey and probably more useful for typical scripts.
  UserScript();
  ~UserScript();

  const std::string& name_space() const { return name_space_; }
  void set_name_space(const std::string& name_space) {
    name_space_ = name_space;
  }

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  const std::string& version() const { return version_; }
  void set_version(const std::string& version) {
    version_ = version;
  }

  const std::string& description() const { return description_; }
  void set_description(const std::string& description) {
    description_ = description;
  }

  // The place in the document to run the script.
  RunLocation run_location() const { return run_location_; }
  void set_run_location(RunLocation location) { run_location_ = location; }

  // Whether to emulate greasemonkey when running this script.
  bool emulate_greasemonkey() const { return emulate_greasemonkey_; }
  void set_emulate_greasemonkey(bool val) { emulate_greasemonkey_ = val; }

  // Whether to match all frames, or only the top one.
  bool match_all_frames() const { return match_all_frames_; }
  void set_match_all_frames(bool val) { match_all_frames_ = val; }

  // The globs, if any, that determine which pages this script runs against.
  // These are only used with "standalone" Greasemonkey-like user scripts.
  const std::vector<std::string>& globs() const { return globs_; }
  void add_glob(const std::string& glob) { globs_.push_back(glob); }
  void clear_globs() { globs_.clear(); }
  const std::vector<std::string>& exclude_globs() const {
    return exclude_globs_;
  }
  void add_exclude_glob(const std::string& glob) {
    exclude_globs_.push_back(glob);
  }
  void clear_exclude_globs() { exclude_globs_.clear(); }

  // The URLPatterns, if any, that determine which pages this script runs
  // against.
  const URLPatternSet& url_patterns() const { return url_set_; }
  void add_url_pattern(const URLPattern& pattern);
  const URLPatternSet& exclude_url_patterns() const {
    return exclude_url_set_;
  }
  void add_exclude_url_pattern(const URLPattern& pattern);

  // List of js scripts for this user script
  FileList& js_scripts() { return js_scripts_; }
  const FileList& js_scripts() const { return js_scripts_; }

  // List of css scripts for this user script
  FileList& css_scripts() { return css_scripts_; }
  const FileList& css_scripts() const { return css_scripts_; }

  const std::string& extension_id() const { return extension_id_; }
  void set_extension_id(const std::string& id) { extension_id_ = id; }

  bool is_incognito_enabled() const { return incognito_enabled_; }
  void set_incognito_enabled(bool enabled) { incognito_enabled_ = enabled; }

  bool is_standalone() const { return extension_id_.empty(); }

  // Returns true if the script should be applied to the specified URL, false
  // otherwise.
  bool MatchesURL(const GURL& url) const;

  // Serialize the UserScript into a pickle. The content of the scripts and
  // paths to UserScript::Files will not be serialized!
  void Pickle(::Pickle* pickle) const;

  // Deserialize the script from a pickle. Note that this always succeeds
  // because presumably we were the one that pickled it, and we did it
  // correctly.
  void Unpickle(const ::Pickle& pickle, PickleIterator* iter);

 private:
  // Pickle helper functions used to pickle the individual types of components.
  void PickleGlobs(::Pickle* pickle,
                   const std::vector<std::string>& globs) const;
  void PickleURLPatternSet(::Pickle* pickle,
                           const URLPatternSet& pattern_list) const;
  void PickleScripts(::Pickle* pickle, const FileList& scripts) const;

  // Unpickle helper functions used to unpickle individual types of components.
  void UnpickleGlobs(const ::Pickle& pickle, PickleIterator* iter,
                     std::vector<std::string>* globs);
  void UnpickleURLPatternSet(const ::Pickle& pickle, PickleIterator* iter,
                             URLPatternSet* pattern_list);
  void UnpickleScripts(const ::Pickle& pickle, PickleIterator* iter,
                       FileList* scripts);

  // The location to run the script inside the document.
  RunLocation run_location_;

  // The namespace of the script. This is used by Greasemonkey in the same way
  // as XML namespaces. Only used when parsing Greasemonkey-style scripts.
  std::string name_space_;

  // The script's name. Only used when parsing Greasemonkey-style scripts.
  std::string name_;

  // A longer description. Only used when parsing Greasemonkey-style scripts.
  std::string description_;

  // A version number of the script. Only used when parsing Greasemonkey-style
  // scripts.
  std::string version_;

  // Greasemonkey-style globs that determine pages to inject the script into.
  // These are only used with standalone scripts.
  std::vector<std::string> globs_;
  std::vector<std::string> exclude_globs_;

  // URLPatterns that determine pages to inject the script into. These are
  // only used with scripts that are part of extensions.
  URLPatternSet url_set_;
  URLPatternSet exclude_url_set_;

  // List of js scripts defined in content_scripts
  FileList js_scripts_;

  // List of css scripts defined in content_scripts
  FileList css_scripts_;

  // The ID of the extension this script is a part of, if any. Can be empty if
  // the script is a "standlone" user script.
  std::string extension_id_;

  // Whether we should try to emulate Greasemonkey's APIs when running this
  // script.
  bool emulate_greasemonkey_;

  // Whether the user script should run in all frames, or only just the top one.
  // Defaults to false.
  bool match_all_frames_;

  // True if the script should be injected into an incognito tab.
  bool incognito_enabled_;
};

typedef std::vector<UserScript> UserScriptList;

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_USER_SCRIPT_H_
