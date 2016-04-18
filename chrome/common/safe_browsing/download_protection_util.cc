// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/download_protection_util.h"

#include <algorithm>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"

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
  EXTENSION_SWF,
  EXTENSION_SPL,
  EXTENSION_APPLICATION,
  EXTENSION_ASP,
  EXTENSION_ASX,
  EXTENSION_CFG,
  EXTENSION_CHI,
  EXTENSION_CHM,
  EXTENSION_CPL,
  EXTENSION_FXP,
  EXTENSION_HLP,
  EXTENSION_HTT,
  EXTENSION_INF,
  EXTENSION_INI,
  EXTENSION_INS,
  EXTENSION_ISP,
  EXTENSION_LNK,
  EXTENSION_LOCAL,
  EXTENSION_MANIFEST,
  EXTENSION_MAU,
  EXTENSION_MMC,
  EXTENSION_MOF,
  EXTENSION_OCX,
  EXTENSION_OPS,
  EXTENSION_PCD,
  EXTENSION_PLG,
  EXTENSION_PRF,
  EXTENSION_PRG,
  EXTENSION_PST,
  EXTENSION_CRT,
  EXTENSION_ADE,
  EXTENSION_ADP,
  EXTENSION_MAD,
  EXTENSION_MAF,
  EXTENSION_MAG,
  EXTENSION_MAM,
  EXTENSION_MAQ,
  EXTENSION_MAR,
  EXTENSION_MAS,
  EXTENSION_MAT,
  EXTENSION_MAV,
  EXTENSION_MAW,
  EXTENSION_MDA,
  EXTENSION_MDB,
  EXTENSION_MDE,
  EXTENSION_MDT,
  EXTENSION_MDW,
  EXTENSION_MDZ,
  EXTENSION_SHB,
  EXTENSION_SHS,
  EXTENSION_VSD,
  EXTENSION_VSMACROS,
  EXTENSION_VSS,
  EXTENSION_VST,
  EXTENSION_VSW,
  EXTENSION_WS,
  EXTENSION_WSC,
  EXTENSION_WSH,
  EXTENSION_XBAP,
  EXTENSION_XNK,
  EXTENSION_JNLP,
  EXTENSION_PL,
  EXTENSION_PY,
  EXTENSION_PYC,
  EXTENSION_PYW,
  EXTENSION_RB,
  EXTENSION_BASH,
  EXTENSION_CSH,
  EXTENSION_KSH,
  EXTENSION_SH,
  EXTENSION_SHAR,
  EXTENSION_TCSH,
  EXTENSION_COMMAND,
  EXTENSION_DEB,
  EXTENSION_RPM,
  EXTENSION_DEX,
  EXTENSION_APPREF_MS,
  EXTENSION_GADGET,
  EXTENSION_EFI,
  EXTENSION_FON,
  EXTENSION_BZIP2,
  EXTENSION_GZIP,
  EXTENSION_TAZ,
  EXTENSION_TBZ,
  EXTENSION_TBZ2,
  EXTENSION_PARTIAL,
  EXTENSION_SVG,
  EXTENSION_XML,
  EXTENSION_XRM_MS,
  EXTENSION_XSL,
  EXTENSION_ACTION,
  EXTENSION_BIN,
  EXTENSION_INX,
  EXTENSION_IPA,
  EXTENSION_ISU,
  EXTENSION_JOB,
  EXTENSION_OUT,
  EXTENSION_PAD,
  EXTENSION_PAF,
  EXTENSION_RGS,
  EXTENSION_U3P,
  EXTENSION_VBSCRIPT,
  EXTENSION_WORKFLOW,
  EXTENSION_001,
  EXTENSION_ACE,
  EXTENSION_ARC,
  EXTENSION_B64,
  EXTENSION_BALZ,
  EXTENSION_BHX,
  EXTENSION_BZ,
  EXTENSION_FAT,
  EXTENSION_HFS,
  EXTENSION_HQX,
  EXTENSION_ISO,
  EXTENSION_LPAQ1,
  EXTENSION_LPAQ5,
  EXTENSION_LPAQ8,
  EXTENSION_MIM,
  EXTENSION_NTFS,
  EXTENSION_PAQ8F,
  EXTENSION_PAQ8JD,
  EXTENSION_PAQ8L,
  EXTENSION_PAQ8O,
  EXTENSION_PEA,
  EXTENSION_PET,
  EXTENSION_PUP,
  EXTENSION_QUAD,
  EXTENSION_R00,
  EXTENSION_R01,
  EXTENSION_R02,
  EXTENSION_R03,
  EXTENSION_R04,
  EXTENSION_R05,
  EXTENSION_R06,
  EXTENSION_R07,
  EXTENSION_R08,
  EXTENSION_R09,
  EXTENSION_R10,
  EXTENSION_R11,
  EXTENSION_R12,
  EXTENSION_R13,
  EXTENSION_R14,
  EXTENSION_R15,
  EXTENSION_R16,
  EXTENSION_R17,
  EXTENSION_R18,
  EXTENSION_R19,
  EXTENSION_R20,
  EXTENSION_R21,
  EXTENSION_R22,
  EXTENSION_R23,
  EXTENSION_R24,
  EXTENSION_R25,
  EXTENSION_R26,
  EXTENSION_R27,
  EXTENSION_R28,
  EXTENSION_R29,
  EXTENSION_SLP,
  EXTENSION_SQUASHFS,
  EXTENSION_SWM,
  EXTENSION_TPZ,
  EXTENSION_TXZ,
  EXTENSION_TZ,
  EXTENSION_UDF,
  EXTENSION_UU,
  EXTENSION_UUE,
  EXTENSION_VHD,
  EXTENSION_VMDK,
  EXTENSION_WRC,
  EXTENSION_XAR,
  EXTENSION_XXE,
  EXTENSION_ZIPX,
  EXTENSION_ZPAQ,
  EXTENSION_RELS,
  EXTENSION_MSG,
  EXTENSION_EML,
  EXTENSION_RTF,
  EXTENSION_VHDX,
  EXTENSION_SEARCH_MS,
  EXTENSION_IMG,
  EXTENSION_SMI,
  EXTENSION_SPARSEBUNDLE,
  EXTENSION_SPARSEIMAGE,
  EXTENSION_CDR,
  EXTENSION_DMGPART,
  EXTENSION_DVDR,
  EXTENSION_DART,
  EXTENSION_DC42,
  EXTENSION_DISKCOPY42,
  EXTENSION_IMGPART,
  EXTENSION_NDIF,
  EXTENSION_UDIF,
  EXTENSION_TOAST,
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
    {FILE_PATH_LITERAL(".001"), EXTENSION_001, true, true},
    {FILE_PATH_LITERAL(".7z"), EXTENSION_7Z, true, true},
    {FILE_PATH_LITERAL(".ace"), EXTENSION_ACE, true, true},
    {FILE_PATH_LITERAL(".action"), EXTENSION_ACTION, false, false},  // UMA.
    {FILE_PATH_LITERAL(".ade"), EXTENSION_ADE, true, false},
    {FILE_PATH_LITERAL(".adp"), EXTENSION_ADP, true, false},
    {FILE_PATH_LITERAL(".apk"), EXTENSION_APK, true, false},
    {FILE_PATH_LITERAL(".app"), EXTENSION_APP, true, false},
    {FILE_PATH_LITERAL(".application"), EXTENSION_APPLICATION, true, false},
    {FILE_PATH_LITERAL(".appref-ms"), EXTENSION_APPREF_MS, true, false},
    {FILE_PATH_LITERAL(".arc"), EXTENSION_ARC, true, true},
    {FILE_PATH_LITERAL(".arj"), EXTENSION_ARJ, true, true},
    {FILE_PATH_LITERAL(".asp"), EXTENSION_ASP, true, false},
    {FILE_PATH_LITERAL(".asx"), EXTENSION_ASX, true, false},
    {FILE_PATH_LITERAL(".b64"), EXTENSION_B64, true, true},
    {FILE_PATH_LITERAL(".balz"), EXTENSION_BALZ, true, true},
    {FILE_PATH_LITERAL(".bas"), EXTENSION_BAS, true, false},
    {FILE_PATH_LITERAL(".bash"), EXTENSION_BASH, true, false},
    {FILE_PATH_LITERAL(".bat"), EXTENSION_BAT, true, false},
    {FILE_PATH_LITERAL(".bhx"), EXTENSION_BHX, true, true},
    {FILE_PATH_LITERAL(".bin"), EXTENSION_BIN, false, false},  // UMA only.
    {FILE_PATH_LITERAL(".bz"), EXTENSION_BZ, true, true},
    {FILE_PATH_LITERAL(".bz2"), EXTENSION_BZ2, true, true},
    {FILE_PATH_LITERAL(".bzip2"), EXTENSION_BZIP2, true, true},
    {FILE_PATH_LITERAL(".cab"), EXTENSION_CAB, true, true},
    {FILE_PATH_LITERAL(".cdr"), EXTENSION_CDR, true, false},
    {FILE_PATH_LITERAL(".cfg"), EXTENSION_CFG, true, false},
    {FILE_PATH_LITERAL(".chi"), EXTENSION_CHI, true, false},
    {FILE_PATH_LITERAL(".chm"), EXTENSION_CHM, true, false},
    {FILE_PATH_LITERAL(".class"), EXTENSION_CLASS, true, false},
    {FILE_PATH_LITERAL(".cmd"), EXTENSION_CMD, true, false},
    {FILE_PATH_LITERAL(".com"), EXTENSION_COM, true, false},
    {FILE_PATH_LITERAL(".command"), EXTENSION_COMMAND, true, false},
    {FILE_PATH_LITERAL(".cpio"), EXTENSION_CPIO, true, true},
    {FILE_PATH_LITERAL(".cpl"), EXTENSION_CPL, true, false},
    {FILE_PATH_LITERAL(".crt"), EXTENSION_CRT, true, false},
    {FILE_PATH_LITERAL(".crx"), EXTENSION_CRX, true, false},
    {FILE_PATH_LITERAL(".csh"), EXTENSION_CSH, true, false},
    {FILE_PATH_LITERAL(".dart"), EXTENSION_DART, true, false},
    {FILE_PATH_LITERAL(".dc42"), EXTENSION_DC42, true, false},
    {FILE_PATH_LITERAL(".deb"), EXTENSION_DEB, true, false},
    {FILE_PATH_LITERAL(".dex"), EXTENSION_DEX, true, false},
    {FILE_PATH_LITERAL(".diskcopy42"), EXTENSION_DISKCOPY42, true, false},
    {FILE_PATH_LITERAL(".dll"), EXTENSION_DLL, true, false},
    {FILE_PATH_LITERAL(".dmg"), EXTENSION_DMG, true, false},
    {FILE_PATH_LITERAL(".dmgpart"), EXTENSION_DMGPART, true, false},
    {FILE_PATH_LITERAL(".drv"), EXTENSION_DRV, true, false},
    {FILE_PATH_LITERAL(".dvdr"), EXTENSION_DVDR, true, false},
    {FILE_PATH_LITERAL(".efi"), EXTENSION_EFI, true, false},
    {FILE_PATH_LITERAL(".eml"), EXTENSION_EML, true, false},
    {FILE_PATH_LITERAL(".exe"), EXTENSION_EXE, true, false},
    {FILE_PATH_LITERAL(".fat"), EXTENSION_FAT, true, true},
    {FILE_PATH_LITERAL(".fon"), EXTENSION_FON, true, false},
    {FILE_PATH_LITERAL(".fxp"), EXTENSION_FXP, true, false},
    {FILE_PATH_LITERAL(".gadget"), EXTENSION_GADGET, true, false},
    {FILE_PATH_LITERAL(".grp"), EXTENSION_GRP, true, false},
    {FILE_PATH_LITERAL(".gz"), EXTENSION_GZ, true, true},
    {FILE_PATH_LITERAL(".gzip"), EXTENSION_GZIP, true, true},
    {FILE_PATH_LITERAL(".hfs"), EXTENSION_HFS, true, true},
    {FILE_PATH_LITERAL(".hlp"), EXTENSION_HLP, true, false},
    {FILE_PATH_LITERAL(".hqx"), EXTENSION_HQX, true, true},
    {FILE_PATH_LITERAL(".hta"), EXTENSION_HTA, true, false},
    {FILE_PATH_LITERAL(".htt"), EXTENSION_HTT, true, false},
    {FILE_PATH_LITERAL(".img"), EXTENSION_IMG, true, false},
    {FILE_PATH_LITERAL(".imgpart"), EXTENSION_IMGPART, true, false},
    {FILE_PATH_LITERAL(".inf"), EXTENSION_INF, true, false},
    {FILE_PATH_LITERAL(".ini"), EXTENSION_INI, true, false},
    {FILE_PATH_LITERAL(".ins"), EXTENSION_INS, true, false},
    {FILE_PATH_LITERAL(".inx"), EXTENSION_INX, false, false},  // UMA only.
    {FILE_PATH_LITERAL(".ipa"), EXTENSION_IPA, false, false},  // UMA only.
    {FILE_PATH_LITERAL(".iso"), EXTENSION_ISO, true, true},
    {FILE_PATH_LITERAL(".isp"), EXTENSION_ISP, true, false},
    {FILE_PATH_LITERAL(".isu"), EXTENSION_ISU, false, false},  // UMA only.
    {FILE_PATH_LITERAL(".jar"), EXTENSION_JAR, true, false},
    {FILE_PATH_LITERAL(".jnlp"), EXTENSION_JNLP, true, false},
    {FILE_PATH_LITERAL(".job"), EXTENSION_JOB, false, false},  // UMA only.
    {FILE_PATH_LITERAL(".js"), EXTENSION_JS, true, false},
    {FILE_PATH_LITERAL(".jse"), EXTENSION_JSE, true, false},
    {FILE_PATH_LITERAL(".ksh"), EXTENSION_KSH, true, false},
    {FILE_PATH_LITERAL(".lha"), EXTENSION_LHA, true, true},
    {FILE_PATH_LITERAL(".lnk"), EXTENSION_LNK, true, false},
    {FILE_PATH_LITERAL(".local"), EXTENSION_LOCAL, true, false},
    {FILE_PATH_LITERAL(".lpaq1"), EXTENSION_LPAQ1, true, true},
    {FILE_PATH_LITERAL(".lpaq5"), EXTENSION_LPAQ5, true, true},
    {FILE_PATH_LITERAL(".lpaq8"), EXTENSION_LPAQ8, true, true},
    {FILE_PATH_LITERAL(".lzh"), EXTENSION_LZH, true, true},
    {FILE_PATH_LITERAL(".lzma"), EXTENSION_LZMA, true, true},
    {FILE_PATH_LITERAL(".mad"), EXTENSION_MAD, true, false},
    {FILE_PATH_LITERAL(".maf"), EXTENSION_MAF, true, false},
    {FILE_PATH_LITERAL(".mag"), EXTENSION_MAG, true, false},
    {FILE_PATH_LITERAL(".mam"), EXTENSION_MAM, true, false},
    {FILE_PATH_LITERAL(".manifest"), EXTENSION_MANIFEST, true, false},
    {FILE_PATH_LITERAL(".maq"), EXTENSION_MAQ, true, false},
    {FILE_PATH_LITERAL(".mar"), EXTENSION_MAR, true, false},
    {FILE_PATH_LITERAL(".mas"), EXTENSION_MAS, true, false},
    {FILE_PATH_LITERAL(".mat"), EXTENSION_MAT, true, false},
    {FILE_PATH_LITERAL(".mau"), EXTENSION_MAU, true, false},
    {FILE_PATH_LITERAL(".mav"), EXTENSION_MAV, true, false},
    {FILE_PATH_LITERAL(".maw"), EXTENSION_MAW, true, false},
    {FILE_PATH_LITERAL(".mda"), EXTENSION_MDA, true, false},
    {FILE_PATH_LITERAL(".mdb"), EXTENSION_MDB, true, false},
    {FILE_PATH_LITERAL(".mde"), EXTENSION_MDE, true, false},
    {FILE_PATH_LITERAL(".mdt"), EXTENSION_MDT, true, false},
    {FILE_PATH_LITERAL(".mdw"), EXTENSION_MDW, true, false},
    {FILE_PATH_LITERAL(".mdz"), EXTENSION_MDZ, true, false},
    {FILE_PATH_LITERAL(".mht"), EXTENSION_MHT, true, false},
    {FILE_PATH_LITERAL(".mhtml"), EXTENSION_MHTML, true, false},
    {FILE_PATH_LITERAL(".mim"), EXTENSION_MIM, true, true},
    {FILE_PATH_LITERAL(".mmc"), EXTENSION_MMC, true, false},
    {FILE_PATH_LITERAL(".mof"), EXTENSION_MOF, true, false},
    {FILE_PATH_LITERAL(".msc"), EXTENSION_MSC, true, false},
    {FILE_PATH_LITERAL(".msg"), EXTENSION_MSG, true, false},
    {FILE_PATH_LITERAL(".msh"), EXTENSION_MSH, true, false},
    {FILE_PATH_LITERAL(".msh1"), EXTENSION_MSH1, true, false},
    {FILE_PATH_LITERAL(".msh1xml"), EXTENSION_MSH1XML, true, false},
    {FILE_PATH_LITERAL(".msh2"), EXTENSION_MSH2, true, false},
    {FILE_PATH_LITERAL(".msh2xml"), EXTENSION_MSH2XML, true, false},
    {FILE_PATH_LITERAL(".mshxml"), EXTENSION_MSHXML, true, false},
    {FILE_PATH_LITERAL(".msi"), EXTENSION_MSI, true, false},
    {FILE_PATH_LITERAL(".msp"), EXTENSION_MSP, true, false},
    {FILE_PATH_LITERAL(".mst"), EXTENSION_MST, true, false},
    {FILE_PATH_LITERAL(".ndif"), EXTENSION_NDIF, true, false},
    {FILE_PATH_LITERAL(".ntfs"), EXTENSION_NTFS, true, true},
    {FILE_PATH_LITERAL(".ocx"), EXTENSION_OCX, true, false},
    {FILE_PATH_LITERAL(".ops"), EXTENSION_OPS, true, false},
    {FILE_PATH_LITERAL(".osx"), EXTENSION_OSX, true, false},
    {FILE_PATH_LITERAL(".out"), EXTENSION_OUT, false, false},  // UMA only.
    {FILE_PATH_LITERAL(".pad"), EXTENSION_PAD, false, false},  // UMA only.
    {FILE_PATH_LITERAL(".paf"), EXTENSION_PAF, false, false},  // UMA only.
    {FILE_PATH_LITERAL(".paq8f"), EXTENSION_PAQ8F, true, true},
    {FILE_PATH_LITERAL(".paq8jd"), EXTENSION_PAQ8JD, true, true},
    {FILE_PATH_LITERAL(".paq8l"), EXTENSION_PAQ8L, true, true},
    {FILE_PATH_LITERAL(".paq8o"), EXTENSION_PAQ8O, true, true},
    {FILE_PATH_LITERAL(".partial"), EXTENSION_PARTIAL, true, false},
    {FILE_PATH_LITERAL(".pcd"), EXTENSION_PCD, true, false},
    {FILE_PATH_LITERAL(".pea"), EXTENSION_PEA, true, true},
    {FILE_PATH_LITERAL(".pet"), EXTENSION_PET, true, true},
    {FILE_PATH_LITERAL(".pif"), EXTENSION_PIF, true, false},
    {FILE_PATH_LITERAL(".pkg"), EXTENSION_PKG, true, false},
    {FILE_PATH_LITERAL(".pl"), EXTENSION_PL, true, false},
    {FILE_PATH_LITERAL(".plg"), EXTENSION_PLG, true, false},
    {FILE_PATH_LITERAL(".prf"), EXTENSION_PRF, true, false},
    {FILE_PATH_LITERAL(".prg"), EXTENSION_PRG, true, false},
    {FILE_PATH_LITERAL(".ps1"), EXTENSION_PS1, true, false},
    {FILE_PATH_LITERAL(".ps1xml"), EXTENSION_PS1XML, true, false},
    {FILE_PATH_LITERAL(".ps2"), EXTENSION_PS2, true, false},
    {FILE_PATH_LITERAL(".ps2xml"), EXTENSION_PS2XML, true, false},
    {FILE_PATH_LITERAL(".psc1"), EXTENSION_PSC1, true, false},
    {FILE_PATH_LITERAL(".psc2"), EXTENSION_PSC2, true, false},
    {FILE_PATH_LITERAL(".pst"), EXTENSION_PST, true, false},
    {FILE_PATH_LITERAL(".pup"), EXTENSION_PUP, true, true},
    {FILE_PATH_LITERAL(".py"), EXTENSION_PY, true, false},
    {FILE_PATH_LITERAL(".pyc"), EXTENSION_PYC, true, false},
    {FILE_PATH_LITERAL(".pyw"), EXTENSION_PYW, true, false},
    {FILE_PATH_LITERAL(".quad"), EXTENSION_QUAD, true, true},
    {FILE_PATH_LITERAL(".r00"), EXTENSION_R00, true, true},
    {FILE_PATH_LITERAL(".r01"), EXTENSION_R01, true, true},
    {FILE_PATH_LITERAL(".r02"), EXTENSION_R02, true, true},
    {FILE_PATH_LITERAL(".r03"), EXTENSION_R03, true, true},
    {FILE_PATH_LITERAL(".r04"), EXTENSION_R04, true, true},
    {FILE_PATH_LITERAL(".r05"), EXTENSION_R05, true, true},
    {FILE_PATH_LITERAL(".r06"), EXTENSION_R06, true, true},
    {FILE_PATH_LITERAL(".r07"), EXTENSION_R07, true, true},
    {FILE_PATH_LITERAL(".r08"), EXTENSION_R08, true, true},
    {FILE_PATH_LITERAL(".r09"), EXTENSION_R09, true, true},
    {FILE_PATH_LITERAL(".r10"), EXTENSION_R10, true, true},
    {FILE_PATH_LITERAL(".r11"), EXTENSION_R11, true, true},
    {FILE_PATH_LITERAL(".r12"), EXTENSION_R12, true, true},
    {FILE_PATH_LITERAL(".r13"), EXTENSION_R13, true, true},
    {FILE_PATH_LITERAL(".r14"), EXTENSION_R14, true, true},
    {FILE_PATH_LITERAL(".r15"), EXTENSION_R15, true, true},
    {FILE_PATH_LITERAL(".r16"), EXTENSION_R16, true, true},
    {FILE_PATH_LITERAL(".r17"), EXTENSION_R17, true, true},
    {FILE_PATH_LITERAL(".r18"), EXTENSION_R18, true, true},
    {FILE_PATH_LITERAL(".r19"), EXTENSION_R19, true, true},
    {FILE_PATH_LITERAL(".r20"), EXTENSION_R20, true, true},
    {FILE_PATH_LITERAL(".r21"), EXTENSION_R21, true, true},
    {FILE_PATH_LITERAL(".r22"), EXTENSION_R22, true, true},
    {FILE_PATH_LITERAL(".r23"), EXTENSION_R23, true, true},
    {FILE_PATH_LITERAL(".r24"), EXTENSION_R24, true, true},
    {FILE_PATH_LITERAL(".r25"), EXTENSION_R25, true, true},
    {FILE_PATH_LITERAL(".r26"), EXTENSION_R26, true, true},
    {FILE_PATH_LITERAL(".r27"), EXTENSION_R27, true, true},
    {FILE_PATH_LITERAL(".r28"), EXTENSION_R28, true, true},
    {FILE_PATH_LITERAL(".r29"), EXTENSION_R29, true, true},
    {FILE_PATH_LITERAL(".rar"), EXTENSION_RAR, true, true},
    {FILE_PATH_LITERAL(".rb"), EXTENSION_RB, true, false},
    {FILE_PATH_LITERAL(".rels"), EXTENSION_RELS, true, false},
    {FILE_PATH_LITERAL(".reg"), EXTENSION_REG, true, false},
    {FILE_PATH_LITERAL(".rgs"), EXTENSION_RGS, false, false},  // UMA only.
    {FILE_PATH_LITERAL(".rpm"), EXTENSION_RPM, true, false},
    {FILE_PATH_LITERAL(".rtf"), EXTENSION_RTF, false, false},  // UMA only.
    {FILE_PATH_LITERAL(".scf"), EXTENSION_SCF, true, false},
    {FILE_PATH_LITERAL(".scr"), EXTENSION_SCR, true, false},
    {FILE_PATH_LITERAL(".sct"), EXTENSION_SCT, true, false},
    {FILE_PATH_LITERAL(".search-ms"), EXTENSION_SEARCH_MS, true, false},
    {FILE_PATH_LITERAL(".sh"), EXTENSION_SH, true, false},
    {FILE_PATH_LITERAL(".shar"), EXTENSION_SHAR, true, false},
    {FILE_PATH_LITERAL(".shb"), EXTENSION_SHB, true, false},
    {FILE_PATH_LITERAL(".shs"), EXTENSION_SHS, true, false},
    {FILE_PATH_LITERAL(".slp"), EXTENSION_SLP, true, true},
    {FILE_PATH_LITERAL(".smi"), EXTENSION_SMI, true, false},
    {FILE_PATH_LITERAL(".sparsebundle"), EXTENSION_SPARSEBUNDLE, true, false},
    {FILE_PATH_LITERAL(".sparseimage"), EXTENSION_SPARSEIMAGE, true, false},
    {FILE_PATH_LITERAL(".spl"), EXTENSION_SPL, true, false},
    {FILE_PATH_LITERAL(".squashfs"), EXTENSION_SQUASHFS, true, true},
    {FILE_PATH_LITERAL(".svg"), EXTENSION_SVG, true, false},
    {FILE_PATH_LITERAL(".swf"), EXTENSION_SWF, true, false},
    {FILE_PATH_LITERAL(".swm"), EXTENSION_SWM, true, true},
    {FILE_PATH_LITERAL(".sys"), EXTENSION_SYS, true, false},
    {FILE_PATH_LITERAL(".tar"), EXTENSION_TAR, true, true},
    {FILE_PATH_LITERAL(".taz"), EXTENSION_TAZ, true, true},
    {FILE_PATH_LITERAL(".tbz"), EXTENSION_TBZ, true, true},
    {FILE_PATH_LITERAL(".tbz2"), EXTENSION_TBZ2, true, true},
    {FILE_PATH_LITERAL(".tcsh"), EXTENSION_TCSH, true, false},
    {FILE_PATH_LITERAL(".tgz"), EXTENSION_TGZ, true, true},
    {FILE_PATH_LITERAL(".toast"), EXTENSION_TOAST, true, false},
    {FILE_PATH_LITERAL(".torrent"), EXTENSION_TORRENT, true, false},
    {FILE_PATH_LITERAL(".tpz"), EXTENSION_TPZ, true, true},
    {FILE_PATH_LITERAL(".txz"), EXTENSION_TXZ, true, true},
    {FILE_PATH_LITERAL(".tz"), EXTENSION_TZ, true, true},
    {FILE_PATH_LITERAL(".u3p"), EXTENSION_U3P, false, false},  // UMA only.
    {FILE_PATH_LITERAL(".udf"), EXTENSION_UDF, true, true},
    {FILE_PATH_LITERAL(".udif"), EXTENSION_UDIF, true, false},
    {FILE_PATH_LITERAL(".url"), EXTENSION_URL, true, false},
    {FILE_PATH_LITERAL(".uu"), EXTENSION_UU, true, true},
    {FILE_PATH_LITERAL(".uue"), EXTENSION_UUE, true, true},
    {FILE_PATH_LITERAL(".vb"), EXTENSION_VB, true, false},
    {FILE_PATH_LITERAL(".vbe"), EXTENSION_VBE, true, false},
    {FILE_PATH_LITERAL(".vbs"), EXTENSION_VBS, true, false},
    {FILE_PATH_LITERAL(".vbscript"), EXTENSION_VBSCRIPT, false, false},  // UMA.
    {FILE_PATH_LITERAL(".vhd"), EXTENSION_VHD, true, true},
    {FILE_PATH_LITERAL(".vhdx"), EXTENSION_VHDX, true, true},
    {FILE_PATH_LITERAL(".vmdk"), EXTENSION_VMDK, true, true},
    {FILE_PATH_LITERAL(".vsd"), EXTENSION_VSD, true, false},
    {FILE_PATH_LITERAL(".vsmacros"), EXTENSION_VSMACROS, true, false},
    {FILE_PATH_LITERAL(".vss"), EXTENSION_VSS, true, false},
    {FILE_PATH_LITERAL(".vst"), EXTENSION_VST, true, false},
    {FILE_PATH_LITERAL(".vsw"), EXTENSION_VSW, true, false},
    {FILE_PATH_LITERAL(".website"), EXTENSION_WEBSITE, true, false},
    {FILE_PATH_LITERAL(".wim"), EXTENSION_WIM, true, true},
    {FILE_PATH_LITERAL(".workflow"), EXTENSION_WORKFLOW, false, false},  // UMA.
    {FILE_PATH_LITERAL(".wrc"), EXTENSION_WRC, true, true},
    {FILE_PATH_LITERAL(".ws"), EXTENSION_WS, true, false},
    {FILE_PATH_LITERAL(".wsc"), EXTENSION_WSC, true, false},
    {FILE_PATH_LITERAL(".wsf"), EXTENSION_WSF, true, false},
    {FILE_PATH_LITERAL(".wsh"), EXTENSION_WSH, true, false},
    {FILE_PATH_LITERAL(".xar"), EXTENSION_XAR, true, true},
    {FILE_PATH_LITERAL(".xbap"), EXTENSION_XBAP, true, false},
    {FILE_PATH_LITERAL(".xml"), EXTENSION_XML, true, false},
    {FILE_PATH_LITERAL(".xnk"), EXTENSION_XNK, true, false},
    {FILE_PATH_LITERAL(".xrm-ms"), EXTENSION_XRM_MS, true, false},
    {FILE_PATH_LITERAL(".xsl"), EXTENSION_XSL, true, false},
    {FILE_PATH_LITERAL(".xxe"), EXTENSION_XXE, true, true},
    {FILE_PATH_LITERAL(".xz"), EXTENSION_XZ, true, true},
    {FILE_PATH_LITERAL(".z"), EXTENSION_Z, true, true},
    {FILE_PATH_LITERAL(".zip"), EXTENSION_ZIP, true, true},
    {FILE_PATH_LITERAL(".zipx"), EXTENSION_ZIPX, true, true},
    {FILE_PATH_LITERAL(".zpaq"), EXTENSION_ZPAQ, true, true},
};

const SafeBrowsingFiletype& GetFileType(const base::FilePath& file) {
  static const SafeBrowsingFiletype kOther = {
    nullptr, EXTENSION_OTHER, false, false
  };

  base::FilePath::StringType extension = GetFileExtension(file);
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

const base::FilePath::StringType GetFileExtension(const base::FilePath& file) {
  // Remove trailing space and period characters from the extension.
  base::FilePath::StringType file_basename = file.BaseName().value();
  base::FilePath::StringPieceType trimmed_filename = base::TrimString(
      file_basename, FILE_PATH_LITERAL(". "), base::TRIM_TRAILING);
  return base::FilePath(trimmed_filename).FinalExtension();
}

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
           file.MatchesExtension(FILE_PATH_LITERAL(".img")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".iso")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".pkg")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".smi")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".osx")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".app")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".cdr")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".dmgpart")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".dvdr")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".dart")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".dc42")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".diskcopy42")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".imgpart")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".ndif")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".udif")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".toast")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".sparsebundle")) ||
           file.MatchesExtension(FILE_PATH_LITERAL(".sparseimage")))
    return ClientDownloadRequest::MAC_EXECUTABLE;
  else if (IsArchiveFile(file))
    return ClientDownloadRequest::ARCHIVE;
  return ClientDownloadRequest::WIN_EXECUTABLE;
}

int GetSBClientDownloadExtensionValueForUMA(const base::FilePath& file) {
  return GetFileType(file).uma_value;
}

}  // namespace download_protection_util
}  // namespace safe_browsing
