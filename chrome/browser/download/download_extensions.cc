// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "chrome/browser/download/download_extensions.h"

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "net/base/mime_util.h"

namespace download_util {

namespace {

enum DownloadAutoOpenHint {
  ALLOW_AUTO_OPEN,

  // The file type should not be allowed to open automatically.
  //
  // Criteria for disallowing a file type from opening automatically:
  //
  // Includes file types that upon opening may either:
  //   * ... execute arbitrary or harmful code with user privileges.
  //   * ... change configuration of the system to cause harmful behavior
  //     immediately or at some time in the future.
  //
  // Doesn't include file types that upon opening:
  //   * ... sufficiently warn the user about the fact that:
  //     - This file was downloaded from the internet.
  //     - Opening it can make specified changes to the system.
  //     (Note that any such warnings need to be displayed prior to the harmful
  //     logic being executed).
  //   * ... does nothing particularly dangerous, despite the act of downloading
  //     itself being dangerous (E.g. .local and .manifest files).
  DISALLOW_AUTO_OPEN,
};

// Guidelines for adding a new dangerous file type:
//
// * Include a comment above the file type that:
//   - Describes the file type.
//   - Justifies why it is considered dangerous if this isn't obvious from the
//     description.
//   - Justifies why the file type is disallowed from auto opening, if
//     necessary.
// * Add the file extension to the kDangerousFileTypes array in
//   download_stats.cc.
//
// TODO(asanka): All file types listed below should have descriptions.
const struct FileType {
  const char* extension;  // Extension sans leading extension separator.
  DownloadDangerLevel danger_level;
  DownloadAutoOpenHint auto_open_hint;
} kDownloadFileTypes[] = {
    // Some files are dangerous on all platforms.

    // Flash files downloaded locally can sometimes access the local filesystem.
    {"swf", DANGEROUS, DISALLOW_AUTO_OPEN},
    {"spl", DANGEROUS, DISALLOW_AUTO_OPEN},

    // Chrome extensions should be obtained through the web store. Allowed to
    // open automatically because Chrome displays a prompt prior to
    // installation.
    {"crx", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Included for parity with kSafeBrowsingFileTypes.
    {"bin", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"rtf", NOT_DANGEROUS, ALLOW_AUTO_OPEN},

    // Archive file types. Not inherently dangerous, but could contain dangerous
    // files. Included for parity with kSafeBrowsingFileTypes.
    {"001", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"7z", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"ace", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"arc", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"arj", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"b64", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"balz", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"bhx", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"bz", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"bz2", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"bzip2", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"cab", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"cpio", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"fat", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"gz", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"gzip", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"hfs", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"hqx", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"iso", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"lha", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"lpaq1", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"lpaq5", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"lpaq8", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"lzh", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"lzma", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"mim", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"ntfs", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"paq8f", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"paq8jd", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"paq8l", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"paq8o", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"pea", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"quad", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r00", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r01", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r02", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r03", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r04", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r05", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r06", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r07", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r08", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r09", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r10", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r11", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r12", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r13", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r14", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r15", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r16", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r17", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r18", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r19", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r20", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r21", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r22", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r23", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r24", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r25", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r26", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r27", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r28", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"r29", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"rar", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"squashfs", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"swm", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"tar", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"taz", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"tbz", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"tbz2", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"tgz", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"tpz", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"txz", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"tz", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"udf", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"uu", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"uue", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"vhd", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"vhdx", NOT_DANGEROUS, ALLOW_AUTO_OPEN},  // Opens in IE, drops MOTW
    {"vmdk", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"wim", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"wrc", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"xar", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"xxe", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"xz", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"z", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"zip", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"zipx", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"zpaq", NOT_DANGEROUS, ALLOW_AUTO_OPEN},

    // Windows, all file categories. The list is in alphabetical order of
    // extensions. Exceptions are made for logical groupings of file types.
    //
    // Some file descriptions are based on
    // https://support.office.com/article/Blocked-attachments-in-Outlook-3811cddc-17c3-4279-a30c-060ba0207372
#if defined(OS_WIN)
    {"ad", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Microsoft Access related.
    {"ade", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Project extension
    {"adp", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Project.
    {"mad", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Module Shortcut.
    {"maf", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mag", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Diagram Shortcut.
    {"mam", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Macro Shortcut.
    {"maq", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Query Shortcut.
    {"mar", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Report Shortcut.
    {"mas", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Stored Procedures.
    {"mat", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Table Shortcut.
    {"mav", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // View Shortcut.
    {"maw", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Data Access Page.
    {"mda", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Access Add-in.
    {"mdb", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Database.
    {"mde", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Database.
    {"mdt", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Add-in Data.
    {"mdw", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Workgroup Information.
    {"mdz", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},  // Wizard Template.

    // Executable Application.
    {"app", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Microsoft ClickOnce depolyment manifest. By default, opens with
    // dfshim.dll which should prompt the user before running untrusted code.
    {"application", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    // ClickOnce application reference. Basically a .lnk for ClickOnce apps.
    {"appref-ms", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Active Server Pages source file.
    {"asp", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Advanced Stream Redirector. Contains a playlist of media files.
    {"asx", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Microsoft Visual Basic source file. Opens by default in an editor.
    {"bas", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Command script.
    {"bat", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    {"cfg", DANGEROUS, ALLOW_AUTO_OPEN},

    // Windows Compiled HTML Help files.
    {"chi", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"chm", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Command script.
    {"cmd", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Windows legacy executable.
    {"com", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Control panel tool. Executable.
    {"cpl", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Signed certificate file.
    {"crt", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Windows executables.
    {"dll", DANGEROUS, DISALLOW_AUTO_OPEN},
    {"drv", DANGEROUS, DISALLOW_AUTO_OPEN},

    // Opens in Outlook. Not common, but could be exploited (CVE-2015-6172)
    {"eml", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Windows executable
    {"exe", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Font file, uses Portable Executable or New Executable format. Not
    // supposed to contain executable code.
    {"fon", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Microsoft FoxPro Compiled Source.
    {"fxp", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Windows Sidebar Gadget (Vista & Win 7). ZIP archive containing html + js.
    // Deprecated by Microsoft. Can run arbitrary code with user privileges.
    // (https://technet.microsoft.com/library/security/2719662)
    {"gadget", DANGEROUS, DISALLOW_AUTO_OPEN},

    // MSProgramGroup (?).
    {"grp", DANGEROUS, ALLOW_AUTO_OPEN},

    // Windows legacy help file format.
    {"hlp", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // HTML Application. Executes as a fully trusted application.
    {"hta", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Hypertext Template File. See https://support.microsoft.com/kb/181689.
    {"htt", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Device installation information.
    {"inf", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Generic configuration file.
    {"ini", DANGEROUS, ALLOW_AUTO_OPEN},

    // Microsoft IIS Internet Communication Settings.
    {"ins", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // InstallShield Compiled Script.
    {"inx", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // InstallShield Uninstaller Script.
    {"isu", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Microsoft IIS Internet Service Provider Settings.
    {"isp", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Windows Task Scheduler Job file. No handler is registered by default, so
    // this is probably normally not dangerous unless saved into the task
    // scheduler directory.
    {"job", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // JavaScript file. May open using Windows Script Host with user level
    // privileges.
    {"js", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // JScript encoded script file. Usually produced by running Microsoft Script
    // Encoder over a .js file.
    // See https://msdn.microsoft.com/library/d14c8zsc.aspx
    {"jse", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Shortcuts. May open anything.
    {"lnk", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // .local files affect DLL search path for .exe file with same base name.
    {"local", DANGEROUS, ALLOW_AUTO_OPEN},

    // While being a generic name, having a .manifest file with the same
    // basename as .exe file (foo.exe + foo.exe.manifest) changes the dll search
    // order for the .exe file. Downloading this kind of file to the users'
    // download directory is almost always the wrong thing to do.
    {"manifest", DANGEROUS, ALLOW_AUTO_OPEN},

    // Media Attachment Unit.
    {"mau", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Multipart HTML.
    {"mht", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mhtml", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    {"mmc", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mof", DANGEROUS, ALLOW_AUTO_OPEN},

    // Microsoft Management Console Snap-in. Contains executable code.
    {"msc", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Opens in Outlook. Not common, but could be exploited (CVE-2015-6172)
    {"msg", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Microsoft Shell.
    {"msh", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"msh1", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"msh2", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"mshxml", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"msh1xml", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"msh2xml", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Windows Installer.
    {"msi", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"msp", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"mst", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // ActiveX Control.
    {"ocx", DANGEROUS, DISALLOW_AUTO_OPEN},

    // Microsoft Office Profile Settings File.
    {"ops", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Portable Application Installer File.
    {"paf", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Extensions that will open in IE even when chrome is set as default
    // browser.
    {"partial", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"xrm-ms", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"rels", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"svg", NOT_DANGEROUS, ALLOW_AUTO_OPEN},
    {"xml", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"xsl", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Microsoft Visual Test.
    {"pcd", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Program Information File. Originally intended to configure execution
    // environment for legacy DOS files. They aren't meant to contain executable
    // code. But Windows may execute a PIF file that is sniffed as a PE file.
    {"pif", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Developer Studio Build Log.
    {"plg", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Windows System File.
    {"prf", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Program File.
    {"prg", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Microsoft Exchange Address Book File. Microsoft Outlook Personal Folder
    // File.
    {"pst", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Microsoft Windows PowerShell.
    {"ps1", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"ps1xml", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"ps2", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"ps2xml", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"psc1", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"psc2", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Registry file. Opening may cause registry settings to change. Users still
    // need to click through a prompt. So we could consider relaxing the
    // DISALLOW_AUTO_OPEN restriction.
    {"reg", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Registry Script Windows.
    {"rgs", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Microsoft Windows Explorer Command.
    // See https://support.microsoft.com/kb/190355 for an example.
    {"scf", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Microsoft Windows Screen Saver.
    {"scr", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Microsoft Windows Script Component. Microsoft FoxPro Screen.
    // A Script Component is a COM component created using script.
    // See https://msdn.microsoft.com/library/aa233148.aspx for an example.
    {"sct", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Microsoft Windows Shortcut into a document.
    // See https://support.microsoft.com/kb/212344
    {"shb", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Shell Scrap Object File.
    {"shs", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // System executable. Windows tries hard to prevent you from opening these
    // types of files.
    {"sys", DANGEROUS, DISALLOW_AUTO_OPEN},

    // U3 Smart Application.
    {"u3p", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Internet Shortcut (new since IE9). Both .url and .website are .ini files
    // that describe a shortcut that points to a URL. They can point at
    // anything. Dropping a download of this type and opening it automatically
    // can in effect sidestep origin restrictions etc.
    {"url", DANGEROUS, DISALLOW_AUTO_OPEN},
    {"website", DANGEROUS, DISALLOW_AUTO_OPEN},

    // VBScript files. My open with Windows Script Host and execute with user
    // privileges.
    {"vb", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"vbe", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"vbs", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    // Some sites claim .vbscript is a valid extension for vbs files.
    {"vbscript", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    {"vsd", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Microsoft Visual Studio Binary-based Macro Project.
    {"vsmacros", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    {"vss", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"vst", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Microsoft Visio Workspace.
    {"vsw", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Windows Script Host related.
    {"ws", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"wsc", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"wsf", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"wsh", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // XAML Browser Application.
    {"xbap", DANGEROUS, DISALLOW_AUTO_OPEN},

    // Microsoft Exchange Public Folder Shortcut.
    {"xnk", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Windows Vista Index Search Data, for local file system.
    // Used to find files landed surreptitiously w/o UI.
    {"search-ms", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
#endif  // OS_WIN

  // Java.
#if !defined(OS_CHROMEOS)
    {"class", DANGEROUS, DISALLOW_AUTO_OPEN},
    {"jar", DANGEROUS, DISALLOW_AUTO_OPEN},
    {"jnlp", DANGEROUS, DISALLOW_AUTO_OPEN},
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    // Scripting languages. (Shells are handled below.)
    {"pl", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"py", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"pyc", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"pyw", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"rb", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Extensible Firmware Interface executable.
    {"efi", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
#endif

  // Shell languages. (OS_ANDROID is OS_POSIX.) OS_WIN shells are handled above.
#if defined(OS_POSIX)
    {"bash", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"csh", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"ksh", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"sh", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"shar", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"tcsh", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
#endif
#if defined(OS_MACOSX)
    // Automator Action.
    {"action", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    {"command", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Automator Workflow.
    {"workflow", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Executable file extensions for Mac.
    {"cdr", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"dart", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"dc42", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"diskcopy42", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"dmg", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"dmgpart", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"dvdr", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"img", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"imgpart", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"ndif", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"smi", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"sparsebundle", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"sparseimage", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"toast", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"udif", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
#endif

  // Package management formats. OS_WIN package formats are handled above.
#if defined(OS_MACOSX) || defined(OS_LINUX)
    {"pkg", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
#endif
#if defined(OS_LINUX)
    {"deb", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"pet", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"pup", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"rpm", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"slp", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // "common" executable file extensions for linux. There's not really much
    // reason to block since they require execute bit to actually run. Included
    // for histograms and to match kSafeBrowsingFileTypes.
    {"out", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"run", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
#endif
#if defined(OS_ANDROID)
    {"dex", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
#endif
};

// FileType for files with an empty extension.
const FileType kEmptyFileType = {nullptr, NOT_DANGEROUS, DISALLOW_AUTO_OPEN};

// Default FileType for non-empty extensions that aren't in the list above.
const FileType kUnknownFileType = {nullptr, NOT_DANGEROUS, ALLOW_AUTO_OPEN};

const FileType& GetFileType(const base::FilePath& path) {
  base::FilePath::StringType extension(path.FinalExtension());
  if (extension.empty())
    return kEmptyFileType;
  if (!base::IsStringASCII(extension))
    return kUnknownFileType;
#if defined(OS_WIN)
  std::string ascii_extension = base::UTF16ToASCII(extension);
#elif defined(OS_POSIX)
  std::string ascii_extension = extension;
#endif

  // Strip out leading dot if it's still there
  if (ascii_extension[0] == base::FilePath::kExtensionSeparator)
    ascii_extension.erase(0, 1);

  for (const auto& file_type : kDownloadFileTypes) {
    if (base::LowerCaseEqualsASCII(ascii_extension, file_type.extension))
      return file_type;
  }

  return kUnknownFileType;
}

}  // namespace

DownloadDangerLevel GetFileDangerLevel(const base::FilePath& path) {
  return GetFileType(path).danger_level;
}

bool IsAllowedToOpenAutomatically(const base::FilePath& path) {
  return GetFileType(path).auto_open_hint == ALLOW_AUTO_OPEN;
}

}  // namespace download_util
