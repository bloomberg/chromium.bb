// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/filename_util.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "net/http/http_content_disposition.h"
#include "url/gurl.h"

namespace net {

namespace {

// what we prepend to get a file URL
static const base::FilePath::CharType kFileURLPrefix[] =
    FILE_PATH_LITERAL("file:///");

void SanitizeGeneratedFileName(base::FilePath::StringType* filename,
                               bool replace_trailing) {
  const base::FilePath::CharType kReplace[] = FILE_PATH_LITERAL("-");
  if (filename->empty())
    return;
  if (replace_trailing) {
    // Handle CreateFile() stripping trailing dots and spaces on filenames
    // http://support.microsoft.com/kb/115827
    size_t length = filename->size();
    size_t pos = filename->find_last_not_of(FILE_PATH_LITERAL(" ."));
    filename->resize((pos == std::string::npos) ? 0 : (pos + 1));
    base::TrimWhitespace(*filename, base::TRIM_TRAILING, filename);
    if (filename->empty())
      return;
    size_t trimmed = length - filename->size();
    if (trimmed)
      filename->insert(filename->end(), trimmed, kReplace[0]);
  }
  base::TrimString(*filename, FILE_PATH_LITERAL("."), filename);
  if (filename->empty())
    return;
  // Replace any path information by changing path separators.
  ReplaceSubstringsAfterOffset(filename, 0, FILE_PATH_LITERAL("/"), kReplace);
  ReplaceSubstringsAfterOffset(filename, 0, FILE_PATH_LITERAL("\\"), kReplace);
}

// Returns the filename determined from the last component of the path portion
// of the URL.  Returns an empty string if the URL doesn't have a path or is
// invalid. If the generated filename is not reliable,
// |should_overwrite_extension| will be set to true, in which case a better
// extension should be determined based on the content type.
std::string GetFileNameFromURL(const GURL& url,
                               const std::string& referrer_charset,
                               bool* should_overwrite_extension) {
  // about: and data: URLs don't have file names, but esp. data: URLs may
  // contain parts that look like ones (i.e., contain a slash).  Therefore we
  // don't attempt to divine a file name out of them.
  if (!url.is_valid() || url.SchemeIs("about") || url.SchemeIs("data"))
    return std::string();

  const std::string unescaped_url_filename = UnescapeURLComponent(
      url.ExtractFileName(),
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);

  // The URL's path should be escaped UTF-8, but may not be.
  std::string decoded_filename = unescaped_url_filename;
  if (!IsStringUTF8(decoded_filename)) {
    // TODO(jshin): this is probably not robust enough. To be sure, we need
    // encoding detection.
    base::string16 utf16_output;
    if (!referrer_charset.empty() &&
        base::CodepageToUTF16(unescaped_url_filename,
                              referrer_charset.c_str(),
                              base::OnStringConversionError::FAIL,
                              &utf16_output)) {
      decoded_filename = base::UTF16ToUTF8(utf16_output);
    } else {
      decoded_filename = base::WideToUTF8(
          base::SysNativeMBToWide(unescaped_url_filename));
    }
  }
  // If the URL contains a (possibly empty) query, assume it is a generator, and
  // allow the determined extension to be overwritten.
  *should_overwrite_extension = !decoded_filename.empty() && url.has_query();

  return decoded_filename;
}

// Returns whether the specified extension is automatically integrated into the
// windows shell.
bool IsShellIntegratedExtension(const base::FilePath::StringType& extension) {
  base::FilePath::StringType extension_lower = StringToLowerASCII(extension);

  // http://msdn.microsoft.com/en-us/library/ms811694.aspx
  // Right-clicking on shortcuts can be magical.
  if ((extension_lower == FILE_PATH_LITERAL("local")) ||
      (extension_lower == FILE_PATH_LITERAL("lnk")))
    return true;

  // http://www.juniper.net/security/auto/vulnerabilities/vuln2612.html
  // Files become magical if they end in a CLSID, so block such extensions.
  if (!extension_lower.empty() &&
      (extension_lower[0] == FILE_PATH_LITERAL('{')) &&
      (extension_lower[extension_lower.length() - 1] == FILE_PATH_LITERAL('}')))
    return true;
  return false;
}

// Returns whether the specified file name is a reserved name on windows.
// This includes names like "com2.zip" (which correspond to devices) and
// desktop.ini and thumbs.db which have special meaning to the windows shell.
bool IsReservedName(const base::FilePath::StringType& filename) {
  // This list is taken from the MSDN article "Naming a file"
  // http://msdn2.microsoft.com/en-us/library/aa365247(VS.85).aspx
  // I also added clock$ because GetSaveFileName seems to consider it as a
  // reserved name too.
  static const char* const known_devices[] = {
    "con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "com5",
    "com6", "com7", "com8", "com9", "lpt1", "lpt2", "lpt3", "lpt4",
    "lpt5", "lpt6", "lpt7", "lpt8", "lpt9", "clock$"
  };
#if defined(OS_WIN)
  std::string filename_lower = StringToLowerASCII(base::WideToUTF8(filename));
#elif defined(OS_POSIX)
  std::string filename_lower = StringToLowerASCII(filename);
#endif

  for (size_t i = 0; i < arraysize(known_devices); ++i) {
    // Exact match.
    if (filename_lower == known_devices[i])
      return true;
    // Starts with "DEVICE.".
    if (filename_lower.find(std::string(known_devices[i]) + ".") == 0)
      return true;
  }

  static const char* const magic_names[] = {
    // These file names are used by the "Customize folder" feature of the shell.
    "desktop.ini",
    "thumbs.db",
  };

  for (size_t i = 0; i < arraysize(magic_names); ++i) {
    if (filename_lower == magic_names[i])
      return true;
  }

  return false;
}


// Examines the current extension in |file_name| and modifies it if necessary in
// order to ensure the filename is safe.  If |file_name| doesn't contain an
// extension or if |ignore_extension| is true, then a new extension will be
// constructed based on the |mime_type|.
//
// We're addressing two things here:
//
// 1) Usability.  If there is no reliable file extension, we want to guess a
//    reasonable file extension based on the content type.
//
// 2) Shell integration.  Some file extensions automatically integrate with the
//    shell.  We block these extensions to prevent a malicious web site from
//    integrating with the user's shell.
void EnsureSafeExtension(const std::string& mime_type,
                         bool ignore_extension,
                         base::FilePath* file_name) {
  // See if our file name already contains an extension.
  base::FilePath::StringType extension = file_name->Extension();
  if (!extension.empty())
    extension.erase(extension.begin());  // Erase preceding '.'.

  if ((ignore_extension || extension.empty()) && !mime_type.empty()) {
    base::FilePath::StringType preferred_mime_extension;
    std::vector<base::FilePath::StringType> all_mime_extensions;
    // The GetPreferredExtensionForMimeType call will end up going to disk.  Do
    // this on another thread to avoid slowing the IO thread.
    // http://crbug.com/61827
    // TODO(asanka): Remove this ScopedAllowIO once all callers have switched
    // over to IO safe threads.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    net::GetPreferredExtensionForMimeType(mime_type, &preferred_mime_extension);
    net::GetExtensionsForMimeType(mime_type, &all_mime_extensions);
    // If the existing extension is in the list of valid extensions for the
    // given type, use it. This avoids doing things like pointlessly renaming
    // "foo.jpg" to "foo.jpeg".
    if (std::find(all_mime_extensions.begin(),
                  all_mime_extensions.end(),
                  extension) != all_mime_extensions.end()) {
      // leave |extension| alone
    } else if (!preferred_mime_extension.empty()) {
      extension = preferred_mime_extension;
    }
  }

#if defined(OS_WIN)
  static const base::FilePath::CharType default_extension[] =
      FILE_PATH_LITERAL("download");

  // Rename shell-integrated extensions.
  // TODO(asanka): Consider stripping out the bad extension and replacing it
  // with the preferred extension for the MIME type if one is available.
  if (IsShellIntegratedExtension(extension))
    extension.assign(default_extension);
#endif

  *file_name = file_name->ReplaceExtension(extension);
}

bool FilePathToString16(const base::FilePath& path, base::string16* converted) {
#if defined(OS_WIN)
  return base::WideToUTF16(
      path.value().c_str(), path.value().size(), converted);
#elif defined(OS_POSIX)
  std::string component8 = path.AsUTF8Unsafe();
  return !component8.empty() &&
         base::UTF8ToUTF16(component8.c_str(), component8.size(), converted);
#endif
}

}  // namespace

GURL FilePathToFileURL(const base::FilePath& path) {
  // Produce a URL like "file:///C:/foo" for a regular file, or
  // "file://///server/path" for UNC. The URL canonicalizer will fix up the
  // latter case to be the canonical UNC form: "file://server/path"
  base::FilePath::StringType url_string(kFileURLPrefix);
  if (!path.IsAbsolute()) {
    base::FilePath current_dir;
    PathService::Get(base::DIR_CURRENT, &current_dir);
    url_string.append(current_dir.value());
    url_string.push_back(base::FilePath::kSeparators[0]);
  }
  url_string.append(path.value());

  // Now do replacement of some characters. Since we assume the input is a
  // literal filename, anything the URL parser might consider special should
  // be escaped here.

  // must be the first substitution since others will introduce percents as the
  // escape character
  ReplaceSubstringsAfterOffset(&url_string, 0,
      FILE_PATH_LITERAL("%"), FILE_PATH_LITERAL("%25"));

  // semicolon is supposed to be some kind of separator according to RFC 2396
  ReplaceSubstringsAfterOffset(&url_string, 0,
      FILE_PATH_LITERAL(";"), FILE_PATH_LITERAL("%3B"));

  ReplaceSubstringsAfterOffset(&url_string, 0,
      FILE_PATH_LITERAL("#"), FILE_PATH_LITERAL("%23"));

  ReplaceSubstringsAfterOffset(&url_string, 0,
      FILE_PATH_LITERAL("?"), FILE_PATH_LITERAL("%3F"));

#if defined(OS_POSIX)
  ReplaceSubstringsAfterOffset(&url_string, 0,
      FILE_PATH_LITERAL("\\"), FILE_PATH_LITERAL("%5C"));
#endif

  return GURL(url_string);
}

bool FileURLToFilePath(const GURL& url, base::FilePath* file_path) {
  *file_path = base::FilePath();
  base::FilePath::StringType& file_path_str =
      const_cast<base::FilePath::StringType&>(file_path->value());
  file_path_str.clear();

  if (!url.is_valid())
    return false;

#if defined(OS_WIN)
  std::string path;
  std::string host = url.host();
  if (host.empty()) {
    // URL contains no host, the path is the filename. In this case, the path
    // will probably be preceeded with a slash, as in "/C:/foo.txt", so we
    // trim out that here.
    path = url.path();
    size_t first_non_slash = path.find_first_not_of("/\\");
    if (first_non_slash != std::string::npos && first_non_slash > 0)
      path.erase(0, first_non_slash);
  } else {
    // URL contains a host: this means it's UNC. We keep the preceeding slash
    // on the path.
    path = "\\\\";
    path.append(host);
    path.append(url.path());
  }
  std::replace(path.begin(), path.end(), '/', '\\');
#else  // defined(OS_WIN)
  // Firefox seems to ignore the "host" of a file url if there is one. That is,
  // file://foo/bar.txt maps to /bar.txt.
  // TODO(dhg): This should probably take into account UNCs which could
  // include a hostname other than localhost or blank
  std::string path = url.path();
#endif  // !defined(OS_WIN)

  if (path.empty())
    return false;

  // GURL stores strings as percent-encoded 8-bit, this will undo if possible.
  path = UnescapeURLComponent(path,
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);

#if defined(OS_WIN)
  if (IsStringUTF8(path)) {
    file_path_str.assign(base::UTF8ToWide(path));
    // We used to try too hard and see if |path| made up entirely of
    // the 1st 256 characters in the Unicode was a zero-extended UTF-16.
    // If so, we converted it to 'Latin-1' and checked if the result was UTF-8.
    // If the check passed, we converted the result to UTF-8.
    // Otherwise, we treated the result as the native OS encoding.
    // However, that led to http://crbug.com/4619 and http://crbug.com/14153
  } else {
    // Not UTF-8, assume encoding is native codepage and we're done. We know we
    // are giving the conversion function a nonempty string, and it may fail if
    // the given string is not in the current encoding and give us an empty
    // string back. We detect this and report failure.
    file_path_str = base::SysNativeMBToWide(path);
  }
#else  // defined(OS_WIN)
  // Collapse multiple path slashes into a single path slash.
  std::string new_path;
  do {
    new_path = path;
    ReplaceSubstringsAfterOffset(&new_path, 0, "//", "/");
    path.swap(new_path);
  } while (new_path != path);

  file_path_str.assign(path);
#endif  // !defined(OS_WIN)

  return !file_path_str.empty();
}

bool IsSafePortablePathComponent(const base::FilePath& component) {
  base::string16 component16;
  base::FilePath::StringType sanitized = component.value();
  SanitizeGeneratedFileName(&sanitized, true);
  base::FilePath::StringType extension = component.Extension();
  if (!extension.empty())
    extension.erase(extension.begin());  // Erase preceding '.'.
  return !component.empty() &&
         (component == component.BaseName()) &&
         (component == component.StripTrailingSeparators()) &&
         FilePathToString16(component, &component16) &&
         file_util::IsFilenameLegal(component16) &&
         !IsShellIntegratedExtension(extension) &&
         (sanitized == component.value()) &&
         !IsReservedName(component.value());
}

bool IsSafePortableRelativePath(const base::FilePath& path) {
  if (path.empty() || path.IsAbsolute() || path.EndsWithSeparator())
    return false;
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);
  if (components.empty())
    return false;
  for (size_t i = 0; i < components.size() - 1; ++i) {
    if (!IsSafePortablePathComponent(base::FilePath(components[i])))
      return false;
  }
  return IsSafePortablePathComponent(path.BaseName());
}

void GenerateSafeFileName(const std::string& mime_type,
                          bool ignore_extension,
                          base::FilePath* file_path) {
  // Make sure we get the right file extension
  EnsureSafeExtension(mime_type, ignore_extension, file_path);

#if defined(OS_WIN)
  // Prepend "_" to the file name if it's a reserved name
  base::FilePath::StringType leaf_name = file_path->BaseName().value();
  DCHECK(!leaf_name.empty());
  if (IsReservedName(leaf_name)) {
    leaf_name = base::FilePath::StringType(FILE_PATH_LITERAL("_")) + leaf_name;
    *file_path = file_path->DirName();
    if (file_path->value() == base::FilePath::kCurrentDirectory) {
      *file_path = base::FilePath(leaf_name);
    } else {
      *file_path = file_path->Append(leaf_name);
    }
  }
#endif
}

base::string16 GetSuggestedFilename(const GURL& url,
                                    const std::string& content_disposition,
                                    const std::string& referrer_charset,
                                    const std::string& suggested_name,
                                    const std::string& mime_type,
                                    const std::string& default_name) {
  // TODO: this function to be updated to match the httpbis recommendations.
  // Talk to abarth for the latest news.

  // We don't translate this fallback string, "download". If localization is
  // needed, the caller should provide localized fallback in |default_name|.
  static const base::FilePath::CharType kFinalFallbackName[] =
    FILE_PATH_LITERAL("download");
  std::string filename;  // In UTF-8
  bool overwrite_extension = false;

  // Try to extract a filename from content-disposition first.
  if (!content_disposition.empty()) {
    HttpContentDisposition header(content_disposition, referrer_charset);
    filename = header.filename();
  }

  // Then try to use the suggested name.
  if (filename.empty() && !suggested_name.empty())
    filename = suggested_name;

  // Now try extracting the filename from the URL.  GetFileNameFromURL() only
  // looks at the last component of the URL and doesn't return the hostname as a
  // failover.
  if (filename.empty())
    filename = GetFileNameFromURL(url, referrer_charset, &overwrite_extension);

  // Finally try the URL hostname, but only if there's no default specified in
  // |default_name|.  Some schemes (e.g.: file:, about:, data:) do not have a
  // host name.
  if (filename.empty() &&
      default_name.empty() &&
      url.is_valid() &&
      !url.host().empty()) {
    // TODO(jungshik) : Decode a 'punycoded' IDN hostname. (bug 1264451)
    filename = url.host();
  }

  bool replace_trailing = false;
  base::FilePath::StringType result_str, default_name_str;
#if defined(OS_WIN)
  replace_trailing = true;
  result_str = base::UTF8ToUTF16(filename);
  default_name_str = base::UTF8ToUTF16(default_name);
#else
  result_str = filename;
  default_name_str = default_name;
#endif
  SanitizeGeneratedFileName(&result_str, replace_trailing);
  if (result_str.find_last_not_of(FILE_PATH_LITERAL("-_")) ==
      base::FilePath::StringType::npos) {
    result_str = !default_name_str.empty() ? default_name_str :
      base::FilePath::StringType(kFinalFallbackName);
    overwrite_extension = false;
  }
  file_util::ReplaceIllegalCharactersInPath(&result_str, '-');
  base::FilePath result(result_str);
  GenerateSafeFileName(mime_type, overwrite_extension, &result);

  base::string16 result16;
  if (!FilePathToString16(result, &result16)) {
    result = base::FilePath(default_name_str);
    if (!FilePathToString16(result, &result16)) {
      result = base::FilePath(kFinalFallbackName);
      FilePathToString16(result, &result16);
    }
  }
  return result16;
}

base::FilePath GenerateFileName(const GURL& url,
                                const std::string& content_disposition,
                                const std::string& referrer_charset,
                                const std::string& suggested_name,
                                const std::string& mime_type,
                                const std::string& default_file_name) {
  base::string16 file_name = GetSuggestedFilename(url,
                                                  content_disposition,
                                                  referrer_charset,
                                                  suggested_name,
                                                  mime_type,
                                                  default_file_name);

#if defined(OS_WIN)
  base::FilePath generated_name(file_name);
#else
  base::FilePath generated_name(
      base::SysWideToNativeMB(base::UTF16ToWide(file_name)));
#endif

#if defined(OS_CHROMEOS)
  // When doing file manager operations on ChromeOS, the file paths get
  // normalized in WebKit layer, so let's ensure downloaded files have
  // normalized names. Otherwise, we won't be able to handle files with NFD
  // utf8 encoded characters in name.
  file_util::NormalizeFileNameEncoding(&generated_name);
#endif

  DCHECK(!generated_name.empty());

  return generated_name;
}

}  // namespace net
