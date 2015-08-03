// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/download_protection_util.h"

#include <algorithm>

#include "base/files/file_path.h"
#include "base/logging.h"

namespace safe_browsing {
namespace download_protection_util {

namespace {

// This enum matches the SBClientDownloadExtensions enum in histograms.xml. If
// you are adding a value here, you should also add to the enum definition in
// histograms.xml. Additions only at the end just in front of EXTENSION_MAX,
// natch.
enum SBClientDownloadExtensions {
  EXTENSION_EXE,
  EXTENSION_MSI,
  EXTENSION_CAB,
  EXTENSION_SYS,
  EXTENSION_SCR,
  EXTENSION_DRV,
  EXTENSION_BAT,
  EXTENSION_ZIP,
  EXTENSION_RAR,
  EXTENSION_DLL,
  EXTENSION_PIF,
  EXTENSION_COM,
  EXTENSION_JAR,
  EXTENSION_CLASS,
  EXTENSION_PDF,
  EXTENSION_VB,
  EXTENSION_REG,
  EXTENSION_GRP,
  EXTENSION_OTHER,  // The "other" bucket. This is in the middle of the enum
                    // due to historical reasons.
  EXTENSION_CRX,
  EXTENSION_APK,
  EXTENSION_DMG,
  EXTENSION_PKG,
  EXTENSION_TORRENT,
  EXTENSION_WEBSITE,
  EXTENSION_URL,
  EXTENSION_VBE,
  EXTENSION_VBS,
  EXTENSION_JS,
  EXTENSION_JSE,
  EXTENSION_MHT,
  EXTENSION_MHTML,
  EXTENSION_MSC,
  EXTENSION_MSP,
  EXTENSION_MST,
  EXTENSION_BAS,
  EXTENSION_HTA,
  EXTENSION_MSH,
  EXTENSION_MSH1,
  EXTENSION_MSH1XML,
  EXTENSION_MSH2,
  EXTENSION_MSH2XML,
  EXTENSION_MSHXML,
  EXTENSION_PS1,
  EXTENSION_PS1XML,
  EXTENSION_PS2,
  EXTENSION_PS2XML,
  EXTENSION_PSC1,
  EXTENSION_PSC2,
  EXTENSION_SCF,
  EXTENSION_SCT,
  EXTENSION_WSF,
  EXTENSION_7Z,
  EXTENSION_XZ,
  EXTENSION_GZ,
  EXTENSION_TGZ,
  EXTENSION_BZ2,
  EXTENSION_TAR,
  EXTENSION_ARJ,
  EXTENSION_LZH,
  EXTENSION_LHA,
  EXTENSION_WIM,
  EXTENSION_Z,
  EXTENSION_LZMA,
  EXTENSION_CPIO,
  EXTENSION_CMD,
  EXTENSION_APP,
  EXTENSION_OSX,

  // New values go above this one.
  EXTENSION_MAX
};

struct SafeBrowsingFiletype {
  const base::FilePath::CharType* const extension;
  int uma_value;
  bool is_supported_binary;
  bool is_archive;
};

const SafeBrowsingFiletype kSafeBrowsingFileTypes[] = {
    // KEEP THIS LIST SORTED!
    {FILE_PATH_LITERAL(".7z"), EXTENSION_7Z, false, true},
    {FILE_PATH_LITERAL(".apk"), EXTENSION_APK, true, false},
    {FILE_PATH_LITERAL(".app"), EXTENSION_APP, true, false},
    {FILE_PATH_LITERAL(".arj"), EXTENSION_ARJ, false, true},
    {FILE_PATH_LITERAL(".bas"), EXTENSION_BAS, true, false},
    {FILE_PATH_LITERAL(".bat"), EXTENSION_BAT, true, false},
    {FILE_PATH_LITERAL(".bz2"), EXTENSION_BZ2, false, true},
    {FILE_PATH_LITERAL(".cab"), EXTENSION_CAB, false, true},
    {FILE_PATH_LITERAL(".cmd"), EXTENSION_CMD, true, false},
    {FILE_PATH_LITERAL(".com"), EXTENSION_COM, true, false},
    {FILE_PATH_LITERAL(".cpio"), EXTENSION_CPIO, false, true},
    {FILE_PATH_LITERAL(".crx"), EXTENSION_CRX, true, false},
    {FILE_PATH_LITERAL(".dll"), EXTENSION_DLL, true, false},
    {FILE_PATH_LITERAL(".dmg"), EXTENSION_DMG, true, false},
    {FILE_PATH_LITERAL(".exe"), EXTENSION_EXE, true, false},
    {FILE_PATH_LITERAL(".gz"), EXTENSION_GZ, false, true},
    {FILE_PATH_LITERAL(".hta"), EXTENSION_HTA, true, false},
    {FILE_PATH_LITERAL(".js"), EXTENSION_JS, true, false},
    {FILE_PATH_LITERAL(".jse"), EXTENSION_JSE, true, false},
    {FILE_PATH_LITERAL(".lha"), EXTENSION_LHA, false, true},
    {FILE_PATH_LITERAL(".lzh"), EXTENSION_LZH, false, true},
    {FILE_PATH_LITERAL(".lzma"), EXTENSION_LZMA, false, true},
    {FILE_PATH_LITERAL(".mht"), EXTENSION_MHT, true, false},
    {FILE_PATH_LITERAL(".mhtml"), EXTENSION_MHTML, true, false},
    {FILE_PATH_LITERAL(".msc"), EXTENSION_MSC, true, false},
    {FILE_PATH_LITERAL(".msh"), EXTENSION_MSH, true, false},
    {FILE_PATH_LITERAL(".msh1"), EXTENSION_MSH1, true, false},
    {FILE_PATH_LITERAL(".msh1xml"), EXTENSION_MSH1XML, true, false},
    {FILE_PATH_LITERAL(".msh2"), EXTENSION_MSH2, true, false},
    {FILE_PATH_LITERAL(".msh2xml"), EXTENSION_MSH2XML, true, false},
    {FILE_PATH_LITERAL(".mshxml"), EXTENSION_MSHXML, true, false},
    {FILE_PATH_LITERAL(".msi"), EXTENSION_MSI, true, false},
    {FILE_PATH_LITERAL(".msp"), EXTENSION_MSP, true, false},
    {FILE_PATH_LITERAL(".mst"), EXTENSION_MST, true, false},
    {FILE_PATH_LITERAL(".osx"), EXTENSION_OSX, true, false},
    {FILE_PATH_LITERAL(".pif"), EXTENSION_PIF, true, false},
    {FILE_PATH_LITERAL(".pkg"), EXTENSION_PKG, true, false},
    {FILE_PATH_LITERAL(".ps1"), EXTENSION_PS1, true, false},
    {FILE_PATH_LITERAL(".ps1xml"), EXTENSION_PS1XML, true, false},
    {FILE_PATH_LITERAL(".ps2"), EXTENSION_PS2, true, false},
    {FILE_PATH_LITERAL(".ps2xml"), EXTENSION_PS2XML, true, false},
    {FILE_PATH_LITERAL(".psc1"), EXTENSION_PSC1, true, false},
    {FILE_PATH_LITERAL(".psc2"), EXTENSION_PSC2, true, false},
    {FILE_PATH_LITERAL(".rar"), EXTENSION_RAR, false, true},
    {FILE_PATH_LITERAL(".reg"), EXTENSION_REG, true, false},
    {FILE_PATH_LITERAL(".scf"), EXTENSION_SCF, true, false},
    {FILE_PATH_LITERAL(".scr"), EXTENSION_SCR, true, false},
    {FILE_PATH_LITERAL(".sct"), EXTENSION_SCT, true, false},
    {FILE_PATH_LITERAL(".tar"), EXTENSION_TAR, false, true},
    {FILE_PATH_LITERAL(".tgz"), EXTENSION_TGZ, false, true},
    {FILE_PATH_LITERAL(".url"), EXTENSION_URL, true, false},
    {FILE_PATH_LITERAL(".vb"), EXTENSION_VB, true, false},
    {FILE_PATH_LITERAL(".vbe"), EXTENSION_VBE, true, false},
    {FILE_PATH_LITERAL(".vbs"), EXTENSION_VBS, true, false},
    {FILE_PATH_LITERAL(".website"), EXTENSION_WEBSITE, true, false},
    {FILE_PATH_LITERAL(".wim"), EXTENSION_WIM, false, true},
    {FILE_PATH_LITERAL(".wsf"), EXTENSION_WSF, true, false},
    {FILE_PATH_LITERAL(".xz"), EXTENSION_XZ, false, true},
    {FILE_PATH_LITERAL(".z"), EXTENSION_Z, false, true},
    {FILE_PATH_LITERAL(".zip"), EXTENSION_ZIP, true, true},
};

const SafeBrowsingFiletype& GetFileType(const base::FilePath& file) {
  static const SafeBrowsingFiletype kOther = {
    nullptr, EXTENSION_OTHER, false, false
  };

  base::FilePath::StringType extension = file.FinalExtension();
  SafeBrowsingFiletype needle = {extension.c_str()};

  const auto begin = kSafeBrowsingFileTypes;
  const auto end = kSafeBrowsingFileTypes + arraysize(kSafeBrowsingFileTypes);
  const auto result = std::lower_bound(
      begin, end, needle,
      [](const SafeBrowsingFiletype& left, const SafeBrowsingFiletype& right) {
        return base::FilePath::CompareLessIgnoreCase(left.extension,
                                                     right.extension);
      });
  if (result == end ||
      !base::FilePath::CompareEqualIgnoreCase(needle.extension,
                                              result->extension))
    return kOther;
  return *result;
}

} // namespace

const int kSBClientDownloadExtensionsMax = EXTENSION_MAX;

bool IsArchiveFile(const base::FilePath& file) {
  // List of interesting archive file formats in kSafeBrowsingFileTypes is by no
  // means exhaustive, but are currently file types that Safe Browsing would
  // like to see pings for due to the possibility of them being used as wrapper
  // formats for malicious payloads.
  return GetFileType(file).is_archive;
}

bool IsSupportedBinaryFile(const base::FilePath& file) {
  return GetFileType(file).is_supported_binary;
}

ClientDownloadRequest::DownloadType GetDownloadType(
    const base::FilePath& file) {
  DCHECK(IsSupportedBinaryFile(file));
  if (file.MatchesExtension(FILE_PATH_LITERAL(".apk")))
    return ClientDownloadRequest::ANDROID_APK;
  else if (file.MatchesExtension(FILE_PATH_LITERAL(".crx")))
    return ClientDownloadRequest::CHROME_EXTENSION;
  else if (file.MatchesExtension(FILE_PATH_LITERAL(".zip")))
    // DownloadProtectionService doesn't send a ClientDownloadRequest for ZIP
    // files unless they contain either executables or archives. The resulting
    // DownloadType is either ZIPPED_EXECUTABLE or ZIPPED_ARCHIVE respectively.
    // This function will return ZIPPED_EXECUTABLE for ZIP files as a
    // placeholder. The correct DownloadType will be determined based on the
    // result of analyzing the ZIP file.
    return ClientDownloadRequest::ZIPPED_EXECUTABLE;
  else if (file.MatchesExtension(FILE_PATH_LITERAL(".dmg")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".pkg")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".osx")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".app")))
    return ClientDownloadRequest::MAC_EXECUTABLE;
  return ClientDownloadRequest::WIN_EXECUTABLE;
}

int GetSBClientDownloadExtensionValueForUMA(const base::FilePath& file) {
  return GetFileType(file).uma_value;
}

}  // namespace download_protection_util
}  // namespace safe_browsing
