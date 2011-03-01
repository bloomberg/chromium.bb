// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENUMERATE_MODULES_MODEL_WIN_H_
#define CHROME_BROWSER_ENUMERATE_MODULES_MODEL_WIN_H_
#pragma once

#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "base/singleton.h"
#include "base/string16.h"
#include "base/timer.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

class EnumerateModulesModel;
class FilePath;
class ListValue;

// A helper class that implements the enumerate module functionality on the File
// thread.
class ModuleEnumerator : public base::RefCountedThreadSafe<ModuleEnumerator> {
 public:
  // What type of module we are dealing with. Loaded modules are modules we
  // detect as loaded in the process at the time of scanning. The others are
  // modules of interest and may or may not be loaded in the process at the
  // time of scan.
  enum ModuleType {
    LOADED_MODULE               = 1 << 0,
    SHELL_EXTENSION             = 1 << 1,
    WINSOCK_MODULE_REGISTRATION = 1 << 2,
  };

  // The blacklist status of the module. Suspected Bad modules have been
  // partially matched (ie. name matches and location, but not description)
  // whereas Confirmed Bad modules have been identified further (ie.
  // AuthentiCode signer matches).
  enum ModuleStatus {
    // This is returned by the matching function when comparing against the
    // blacklist and the module does not match the current entry in the
    // blacklist.
    NOT_MATCHED,
    // The module is not on the blacklist. Assume it is good.
    GOOD,
    // Module is a suspected bad module.
    SUSPECTED_BAD,
    // Module is a bad bad dog.
    CONFIRMED_BAD,
  };

  // A bitmask with the possible resolutions for bad modules.
  enum RecommendedAction {
    NONE          = 0,
    INVESTIGATING = 1 << 0,
    UNINSTALL     = 1 << 1,
    DISABLE       = 1 << 2,
    UPDATE        = 1 << 3,
    SEE_LINK      = 1 << 4,
  };

  // The structure we populate when enumerating modules.
  struct Module {
    // The type of module found
    ModuleType type;
    // The module status (benign/bad/etc).
    ModuleStatus status;
    // The module path, not including filename.
    string16 location;
    // The name of the module (filename).
    string16 name;
    // The name of the product the module belongs to.
    string16 product_name;
    // The module file description.
    string16 description;
    // The module version.
    string16 version;
    // The signer of the digital certificate for the module.
    string16 digital_signer;
    // The help tips bitmask.
    RecommendedAction recommended_action;
    // The duplicate count within each category of modules.
    int duplicate_count;
    // Whether this module has been normalized (necessary before checking it
    // against blacklist).
    bool normalized;
  };

  // A vector typedef of all modules enumerated.
  typedef std::vector<Module> ModulesVector;

  // A structure we populate with the blacklist entries.
  struct BlacklistEntry {
    const char* filename;
    const char* location;
    const char* desc_or_signer;
    const char* version_from;  // Version where conflict started.
    const char* version_to;    // First version that works.
    RecommendedAction help_tip;
  };

  // A static function that normalizes the module information in the |module|
  // struct. Module information needs to be normalized before comparing against
  // the blacklist. This is because the same module can be described in many
  // different ways, ie. file paths can be presented in long/short name form,
  // and are not case sensitive on Windows. Also, the version string returned
  // can include appended text, which we don't want to use during comparison
  // against the blacklist.
  static void NormalizeModule(Module* module);

  // A static function that checks whether |module| has been |blacklisted|.
  static ModuleStatus Match(const Module& module,
                            const BlacklistEntry& blacklisted);

  explicit ModuleEnumerator(EnumerateModulesModel* observer);
  ~ModuleEnumerator();

  // Start scanning the loaded module list (if a scan is not already in
  // progress). This function does not block while reading the module list
  // (unless we are in limited_mode, see below), and will notify when done
  // through the MODULE_LIST_ENUMERATED notification.
  // The process will also send MODULE_INCOMPATIBILITY_DETECTED if an
  // incompatible module was detected.
  // When in |limited_mode|, this function will not leverage the File thread
  // to run asynchronously and will therefore block until scanning is done
  // (and will also not send out any notifications).
  void ScanNow(ModulesVector* list, bool limited_mode);

 private:
  FRIEND_TEST_ALL_PREFIXES(EnumerateModulesTest, CollapsePath);

  // The (currently) hard coded blacklist of known bad modules.
  static const BlacklistEntry kModuleBlacklist[];

  // This function does the actual file scanning work on the FILE thread (or
  // block the main thread when in limited_mode). It enumerates all loaded
  // modules in the process and other modules of interest, such as the
  // registered Winsock LSP modules and stores them in |enumerated_modules_|.
  // It then normalizes the module info and matches them against a blacklist
  // of known bad modules. Finally, it calls ReportBack to let the observer
  // know we are done.
  void ScanImpl();

  // Enumerate all modules loaded into the Chrome process.
  void EnumerateLoadedModules();

  // Enumerate all registered Windows shell extensions.
  void EnumerateShellExtensions();

  // Enumerate all registered Winsock LSP modules.
  void EnumerateWinsockModules();

  // Reads the registered shell extensions found under |parent| key in the
  // registry.
  void ReadShellExtensions(HKEY parent);

  // Given a |module|, initializes the structure and loads additional
  // information using the location field of the module.
  void PopulateModuleInformation(Module* module);

  // Checks the module list to see if a |module| of the same type, location
  // and name has been added before and if so, increments its duplication
  // counter. If it doesn't appear in the list, it is added.
  void AddToListWithoutDuplicating(const Module&);

  // Builds up a vector of path values mapping to environment variable,
  // with pairs like [c:\windows\, %systemroot%]. This is later used to
  // collapse paths like c:\windows\system32 into %systemroot%\system32, which
  // we can use for comparison against our blacklist (which uses only env vars).
  // NOTE: The vector will not contain an exhaustive list of environment
  // variables, only the ones currently found on the blacklist or ones that are
  // likely to appear there.
  void PreparePathMappings();

  // For a given |module|, collapse the path from c:\windows to %systemroot%,
  // based on the |path_mapping_| vector.
  void CollapsePath(Module* module);

  // Takes each module in the |enumerated_modules_| vector and matches it
  // against a fixed blacklist of bad and suspected bad modules.
  void MatchAgainstBlacklist();

  // This function executes on the UI thread when the scanning and matching
  // process is done. It notifies the observer.
  void ReportBack();

  // Given a filename, returns the Subject (who signed it) retrieved from
  // the digital signature (Authenticode).
  string16 GetSubjectNameFromDigitalSignature(const FilePath& filename);

  // The typedef for the vector that maps a regular file path to %env_var%.
  typedef std::vector< std::pair<string16, string16> > PathMapping;

  // The vector of paths to %env_var%, used to account for differences in
  // where people keep there files, c:\windows vs. d:\windows, etc.
  PathMapping path_mapping_;

  // The vector containing all the enumerated modules (loaded and modules of
  // interest).
  ModulesVector* enumerated_modules_;

  // The observer, who needs to be notified when we are done.
  EnumerateModulesModel* observer_;

  // See limited_mode below.
  bool limited_mode_;

  // The thread that we need to call back on to report that we are done.
  BrowserThread::ID callback_thread_id_;

  DISALLOW_COPY_AND_ASSIGN(ModuleEnumerator);
};

// This is a singleton class that enumerates all modules loaded into Chrome,
// both currently loaded modules (called DLLs on Windows) and modules 'of
// interest', such as WinSock LSP modules. This class also marks each module
// as benign or suspected bad or outright bad, using a supplied blacklist that
// is currently hard-coded.
//
// To use this class, grab the singleton pointer and call ScanNow().
// Then wait to get notified through MODULE_LIST_ENUMERATED when the list is
// ready.
//
// This class can be used on the UI thread as it asynchronously offloads the
// file work over to the FILE thread and reports back to the caller with a
// notification.
class EnumerateModulesModel {
 public:
  static EnumerateModulesModel* GetInstance();

  // Returns the number of suspected bad modules found in the last scan.
  // Returns 0 if no scan has taken place yet.
  int suspected_bad_modules_detected() {
    return suspected_bad_modules_detected_;
  }

  // Returns the number of confirmed bad modules found in the last scan.
  // Returns 0 if no scan has taken place yet.
  int confirmed_bad_modules_detected() {
    return confirmed_bad_modules_detected_;
  }

  // Set to true when we the scanning process can not rely on certain Chrome
  // services to exists.
  void set_limited_mode(bool limited_mode) {
    limited_mode_ = limited_mode;
  }

  // Asynchronously start the scan for the loaded module list, except when in
  // limited_mode (in which case it blocks).
  void ScanNow();

  // Gets the whole module list as a ListValue.
  ListValue* GetModuleList();

 private:
  friend struct DefaultSingletonTraits<EnumerateModulesModel>;
  friend class ModuleEnumerator;

  EnumerateModulesModel();
  virtual ~EnumerateModulesModel();

  // Called on the UI thread when the helper class is done scanning.
  void DoneScanning();

  // Constructs a Help Center article URL for help with a particular module.
  // The module must have the SEE_LINK attribute for |recommended_action| set,
  // otherwise this returns a blank string.
  GURL ConstructHelpCenterUrl(const ModuleEnumerator::Module& module);

  // The vector containing all the modules enumerated. Will be normalized and
  // any bad modules will be marked.
  ModuleEnumerator::ModulesVector enumerated_modules_;

  // The object responsible for enumerating the modules on the File thread.
  scoped_refptr<ModuleEnumerator> module_enumerator_;

  // When this singleton object is constructed we go and fire off this timer to
  // start scanning for modules after a certain amount of time has passed.
  base::OneShotTimer<EnumerateModulesModel> check_modules_timer_;

  // While normally |false|, this mode can be set to indicate that the scanning
  // process should not rely on certain services normally available to Chrome,
  // such as the resource bundle and the notification system, not to mention
  // having multiple threads. This mode is useful during diagnostics, which
  // runs without firing up all necessary Chrome services first.
  bool limited_mode_;

  // True if we are currently scanning for modules.
  bool scanning_;

  // The number of confirmed bad modules (not including suspected bad ones)
  // found during last scan.
  int confirmed_bad_modules_detected_;

  // The number of suspected bad modules (not including confirmed bad ones)
  // found during last scan.
  int suspected_bad_modules_detected_;

  DISALLOW_COPY_AND_ASSIGN(EnumerateModulesModel);
};

#endif  // CHROME_BROWSER_ENUMERATE_MODULES_MODEL_WIN_H_
