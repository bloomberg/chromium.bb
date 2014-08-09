# This file is automatically processed to create .DEPS.git which is the file
# that gclient uses under git.
#
# See http://code.google.com/p/chromium/wiki/UsingGit
#
# To test manually, run:
#   python tools/deps2git/deps2git.py -o .DEPS.git -w <gclientdir>
# where <gcliendir> is the absolute path to the directory containing the
# .gclient file (the parent of "src").
#
# Then commit .DEPS.git locally (gclient doesn't like dirty trees) and run
#   gclient sync
# Verify the thing happened you wanted. Then revert your .DEPS.git change
# DO NOT CHECK IN CHANGES TO .DEPS.git upstream. It will be automatically
# updated by a bot when you modify this one.
#
# When adding a new dependency, please update the top-level .gitignore file
# to list the dependency's destination directory.

vars = {
  # Use this googlecode_url variable only if there is an internal mirror for it.
  # If you do not know, use the full path while defining your new deps entry.
  "googlecode_url": "http://%s.googlecode.com/svn",
  "sourceforge_url": "http://svn.code.sf.net/p/%(repo)s/code",
  "llvm_url": "http://src.chromium.org/llvm-project",
  "llvm_git": "https://llvm.googlesource.com",
  "libcxx_revision": "206024",
  "libcxxabi_revision": "206024",
  "webkit_trunk": "http://src.chromium.org/blink/trunk",
  "nacl_trunk": "http://src.chromium.org/native_client/trunk",
  "webkit_revision": "179874",
  "chromium_git": "https://chromium.googlesource.com",
  "chromiumos_git": "https://chromium.googlesource.com/chromiumos",
  "pdfium_git": "https://pdfium.googlesource.com",
  "skia_git": "https://skia.googlesource.com",
  "boringssl_git": "https://boringssl.googlesource.com",
  "swig_revision": "230490",
  "nacl_revision": "13592",
  # After changing nacl_revision, run 'glient sync' and check native_client/DEPS
  # to update other nacl_*_revision's.
  "nacl_tools_revision": "13077",  # native_client/DEPS: tools_rev
  "google_toolbox_for_mac_revision": "662",
  "libaddressinput_revision": "316",
  "libphonenumber_revision": "621",
  "libvpx_revision": "287125",
  "lss_revision": "31",
  "sfntly_revision": "239",
  "lighttpd_revision": "33737",
  "skia_revision": "3b1c3d2c6160f714c4ebada05dd834f88fe5fd00",
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling Skia
  # and V8 without interference from each other.
  "v8_branch": "trunk",
  "v8_revision": "22960",
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling WebRTC
  # and V8 without interference from each other.
  "webrtc_revision": "6825",
  "jsoncpp_revision": "248",
  "nss_revision": "287121",
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling swarming_client
  # and whatever else without interference from each other.
  "swarming_revision": "bbf1fcca7932d92cca9d7dab46ea271a7f6d61fb",
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling openssl
  # and whatever else without interference from each other.
  "openssl_revision": "284247",
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling ANGLE
  # and whatever else without interference from each other.
  "angle_revision": "daf565720b9e26a004e8716a3f52d4a63b988217",
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling build tools
  # and whatever else without interference from each other.
  "buildtools_revision": "33f3e1c4e0d125641524c0a5955594b1c22c804a",
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling PDFIum
  # and whatever else without interference from each other.
  "pdfium_revision": "df449c0eb4b1fb5583da71265faf50a9a520be3c",
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling openmax_dl
  # and whatever else without interference from each other.
  "openmax_dl_revision": "6777",
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling BoringSSL
  # and whatever else without interference from each other.
  "boringssl_revision": "c3d79605ab06cffa87877bcfe0792f767bde8b90",
}

deps = {
  "src/breakpad/src":
    (Var("googlecode_url") % "google-breakpad") + "/trunk/src@1359",

  "src/buildtools":
    Var("chromium_git") + "/chromium/buildtools.git@" +
     Var("buildtools_revision"),

  "src/sdch/open-vcdiff":
    (Var("googlecode_url") % "open-vcdiff") + "/trunk@42",

  "src/testing/gtest":
    (Var("googlecode_url") % "googletest") + "/trunk@692",

  "src/testing/gmock":
    (Var("googlecode_url") % "googlemock") + "/trunk@485",

  "src/third_party/angle":
    Var("chromium_git") + "/angle/angle.git@" + Var("angle_revision"),

  "src/third_party/colorama/src":
    Var("chromium_git") + "/external/colorama.git@799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8",

  "src/third_party/trace-viewer":
    (Var("googlecode_url") % "trace-viewer") + "/trunk@1449",

  "src/third_party/WebKit":
    Var("webkit_trunk") + "@" + Var("webkit_revision"),

  "src/third_party/WebKit/LayoutTests/w3c/web-platform-tests":
    Var("chromium_git") +
    "/external/w3c/web-platform-tests.git@6bed4516fe8522d65512c76ef02e4f0ae8234395",

  "src/third_party/WebKit/LayoutTests/w3c/csswg-test":
    Var("chromium_git") +
    "/external/w3c/csswg-test.git@bacbb4a8dca702cd86646761fde96793db13d4f1",

  "src/third_party/icu":
    "/trunk/deps/third_party/icu52@287122",

  "src/third_party/libexif/sources":
    "/trunk/deps/third_party/libexif/sources@265008",

  "src/third_party/hunspell":
    "/trunk/deps/third_party/hunspell@287123",

  "src/third_party/hunspell_dictionaries":
    "/trunk/deps/third_party/hunspell_dictionaries@255132",

  "src/third_party/safe_browsing/testing":
    (Var("googlecode_url") % "google-safe-browsing") + "/trunk/testing@112",

  "src/third_party/cacheinvalidation/src":
    (Var("googlecode_url") % "google-cache-invalidation-api") +
    "/trunk/src@335",

  "src/third_party/leveldatabase/src":
    (Var("googlecode_url") % "leveldb") + "/trunk@80",

  "src/third_party/libc++/trunk":
    Var("llvm_url") + "/libcxx/trunk@" + Var("libcxx_revision"),

  "src/third_party/libc++abi/trunk":
    Var("llvm_url") + "/libcxxabi/trunk@" + Var("libcxxabi_revision"),

  "src/third_party/snappy/src":
    (Var("googlecode_url") % "snappy") + "/trunk@80",

  "src/tools/grit":
    (Var("googlecode_url") % "grit-i18n") + "/trunk@172",

  "src/tools/gyp":
    (Var("googlecode_url") % "gyp") + "/trunk@1959",

  "src/tools/swarming_client":
    Var("chromium_git") + "/external/swarming.client.git@" +
        Var("swarming_revision"),

  "src/v8":
    (Var("googlecode_url") % "v8") + "/" + Var("v8_branch") + "@" +
    Var("v8_revision"),

  "src/native_client":
    Var("nacl_trunk") + "/src/native_client@" + Var("nacl_revision"),

  "src/chrome/test/data/extensions/api_test/permissions/nacl_enabled/bin":
    Var("nacl_trunk") + "/src/native_client/tests/prebuilt@" +
    Var("nacl_revision"),

  "src/third_party/sfntly/cpp/src":
    (Var("googlecode_url") % "sfntly") + "/trunk/cpp/src@" +
    Var("sfntly_revision"),

  "src/third_party/skia":
    Var("chromium_git") + "/skia.git@" + Var("skia_revision"),

  "src/third_party/ots":
    (Var("googlecode_url") % "ots") + "/trunk@115",

  "src/third_party/brotli/src":
    Var("chromium_git") +
    "/external/font-compression-reference.git@6cef49677dc4c650ef6e3f56041e0a41803afa8c",

  "src/tools/page_cycler/acid3":
    "/trunk/deps/page_cycler/acid3@171600",

  "src/chrome/test/data/perf/canvas_bench":
    "/trunk/deps/canvas_bench@122605",

  "src/chrome/test/data/perf/frame_rate/content":
    "/trunk/deps/frame_rate/content@93671",

  "src/third_party/bidichecker":
    (Var("googlecode_url") % "bidichecker") + "/trunk/lib@4",

  "src/third_party/webgl/src":
    Var("chromium_git") +
    "/external/khronosgroup/webgl.git@f65caf9f2488ad7c7f9637b0ac864389f2fc0f16",

  "src/third_party/swig/Lib":
    "/trunk/deps/third_party/swig/Lib@" + Var("swig_revision"),

  "src/third_party/webdriver/pylib":
    (Var("googlecode_url") % "selenium") + "/trunk/py@18456",

  "src/third_party/libvpx":
    "/trunk/deps/third_party/libvpx@" +
    Var("libvpx_revision"),

  "src/third_party/ffmpeg":
    Var("chromium_git") +
    "/chromium/third_party/ffmpeg.git@5ab896ad9dffd4a78e3c310de8e3507ded3e3239",

  "src/third_party/libjingle/source/talk":
    (Var("googlecode_url") % "webrtc") + "/trunk/talk@" +
    Var("webrtc_revision"),

  "src/third_party/usrsctp/usrsctplib":
    (Var("googlecode_url") % "sctp-refimpl") +
    "/trunk/KERN/usrsctp/usrsctplib@8912",

  "src/third_party/libsrtp":
    "/trunk/deps/third_party/libsrtp@283418",

  "src/third_party/speex":
    "/trunk/deps/third_party/speex@272757",

  "src/third_party/yasm/source/patched-yasm":
    "/trunk/deps/third_party/yasm/patched-yasm@167605",

  "src/third_party/libjpeg_turbo":
    "/trunk/deps/third_party/libjpeg_turbo@272637",

  "src/third_party/flac":
    "/trunk/deps/third_party/flac@287124",

  "src/third_party/pyftpdlib/src":
    (Var("googlecode_url") % "pyftpdlib") + "/trunk@977",

  "src/third_party/scons-2.0.1":
    Var("nacl_trunk") + "/src/third_party/scons-2.0.1@" +
        Var("nacl_tools_revision"),

  "src/third_party/webrtc":
    (Var("googlecode_url") % "webrtc") + "/trunk/webrtc@" + Var("webrtc_revision"),

  "src/third_party/openmax_dl":
    (Var("googlecode_url") % "webrtc") + "/deps/third_party/openmax@" + Var("openmax_dl_revision"),

  "src/third_party/jsoncpp/source/include":
    (Var("sourceforge_url") % {"repo": "jsoncpp"}) +
        "/trunk/jsoncpp/include@" + Var("jsoncpp_revision"),

  "src/third_party/jsoncpp/source/src/lib_json":
    (Var("sourceforge_url") % {"repo": "jsoncpp"}) +
        "/trunk/jsoncpp/src/lib_json@" + Var("jsoncpp_revision"),

  "src/third_party/libyuv":
    (Var("googlecode_url") % "libyuv") + "/trunk@1035",

  "src/third_party/smhasher/src":
    (Var("googlecode_url") % "smhasher") + "/trunk@152",

  "src/third_party/libaddressinput/src/cpp":
    (Var("googlecode_url") % "libaddressinput") + "/trunk/cpp@" +
        Var("libaddressinput_revision"),
  "src/third_party/libaddressinput/src/testdata":
    (Var("googlecode_url") % "libaddressinput") + "/trunk/testdata@" +
        Var("libaddressinput_revision"),

  "src/third_party/libphonenumber/src/phonenumbers":
    (Var("googlecode_url") % "libphonenumber") +
        "/trunk/cpp/src/phonenumbers@" + Var("libphonenumber_revision"),
  "src/third_party/libphonenumber/src/test":
    (Var("googlecode_url") % "libphonenumber") + "/trunk/cpp/test@" +
        Var("libphonenumber_revision"),
  "src/third_party/libphonenumber/src/resources":
    (Var("googlecode_url") % "libphonenumber") + "/trunk/resources@" +
        Var("libphonenumber_revision"),

  "src/tools/deps2git":
    "/trunk/tools/deps2git@276439",

  "src/third_party/webpagereplay":
    Var("chromium_git") + "/external/web-page-replay.git@" +
    "b62c02d3b64cf00a2f65a82cca0721aa42c3d6ad",

  "src/third_party/pywebsocket/src":
    (Var("googlecode_url") % "pywebsocket") + "/trunk/src@790",

  "src/third_party/opus/src":
    "/trunk/deps/third_party/opus@256783",

  "src/media/cdm/ppapi/api":
    "/trunk/deps/cdm@288172",

  "src/third_party/mesa/src":
    "/trunk/deps/third_party/mesa@265279",

  "src/third_party/cld_2/src":
    (Var("googlecode_url") % "cld2") + "/trunk@165",

  "src/chrome/browser/resources/pdf/html_office":
     Var("chromium_git") +
         "/chromium/html-office-public.git@eeff97614f65e0578529490d44d412032c3d7359",

  "src/third_party/libwebm/source":
    Var("chromium_git") +
      "/webm/libwebm.git@0d4cb404ea4195e5e21d04db2c955615535ce62e",

  "src/third_party/openssl":
    "/trunk/deps/third_party/openssl@" + Var("openssl_revision"),

  "src/third_party/pdfium":
    Var("pdfium_git") + "/pdfium.git@" + Var("pdfium_revision"),

  "src/third_party/boringssl/src":
    Var("boringssl_git") + "/boringssl.git@" + Var("boringssl_revision"),
}


deps_os = {
  "win": {
    "src/chrome/tools/test/reference_build/chrome_win":
      "/trunk/deps/reference_builds/chrome_win@273882",

    "src/third_party/cygwin":
      "/trunk/deps/third_party/cygwin@231940",

    "src/third_party/psyco_win32":
      "/trunk/deps/third_party/psyco_win32@237949",

    "src/third_party/bison":
      "/trunk/deps/third_party/bison@147303",

    "src/third_party/gperf":
      "/trunk/deps/third_party/gperf@147304",

    "src/third_party/perl":
      "/trunk/deps/third_party/perl@147900",

    "src/third_party/lighttpd":
      "/trunk/deps/third_party/lighttpd@" + Var("lighttpd_revision"),

    # Parses Windows PE/COFF executable format.
    "src/third_party/pefile":
      Var("chromium_git") + '/external/pefile.git@' +
      '72c6ae42396cb913bcab63c15585dc3b5c3f92f1',

    # NSS, for SSLClientSocketNSS.
    "src/third_party/nss":
      "/trunk/deps/third_party/nss@" + Var("nss_revision"),

    "src/third_party/swig/win":
      "/trunk/deps/third_party/swig/win@" + Var("swig_revision"),

    # GNU binutils assembler for x86-32.
    "src/third_party/gnu_binutils":
      (Var("nacl_trunk") + "/deps/third_party/gnu_binutils@" +
       Var("nacl_tools_revision")),
    # GNU binutils assembler for x86-64.
    "src/third_party/mingw-w64/mingw/bin":
      (Var("nacl_trunk") + "/deps/third_party/mingw-w64/mingw/bin@" +
       Var("nacl_tools_revision")),

    # Dependencies used by libjpeg-turbo
    "src/third_party/yasm/binaries":
      "/trunk/deps/third_party/yasm/binaries@154708",

    # Binaries for nacl sdk.
    "src/third_party/nacl_sdk_binaries":
      "/trunk/deps/third_party/nacl_sdk_binaries@111576",
  },
  "ios": {
    "src/third_party/google_toolbox_for_mac/src":
      (Var("googlecode_url") % "google-toolbox-for-mac") + "/trunk@" +
      Var("google_toolbox_for_mac_revision"),

    "src/third_party/nss":
      "/trunk/deps/third_party/nss@" + Var("nss_revision"),

    # class-dump utility to generate header files for undocumented SDKs
    "src/testing/iossim/third_party/class-dump":
      "/trunk/deps/third_party/class-dump@199203",

    # Code that's not needed due to not building everything
    "src/build/util/support": None,
    "src/chrome/test/data/extensions/api_test/permissions/nacl_enabled/bin": None,
    "src/chrome/test/data/perf/canvas_bench": None,
    "src/chrome/test/data/perf/frame_rate/content": None,
    "src/media/cdm/ppapi/api": None,
    "src/native_client": None,
    "src/native_client/src/third_party/ppapi": None,
    "src/third_party/angle": None,
    "src/third_party/bidichecker": None,
    "src/third_party/brotli/src": None,
    "src/third_party/cld_2/src": None,
    "src/third_party/ffmpeg": None,
    "src/third_party/hunspell_dictionaries": None,
    "src/third_party/hunspell": None,
    "src/third_party/libc++/trunk": None,
    "src/third_party/libc++abi/trunk": None,
    "src/third_party/libexif/sources": None,
    "src/third_party/libjpeg_turbo": None,
    "src/third_party/libsrtp": None,
    "src/third_party/mesa/src": None,
    "src/third_party/opus/src": None,
    "src/third_party/openmax_dl": None,
    "src/third_party/ots": None,
    "src/third_party/pymox/src": None,
    "src/third_party/safe_browsing/testing": None,
    "src/third_party/scons-2.0.1": None,
    "src/third_party/sfntly/cpp/src": None,
    "src/third_party/speex": None,
    "src/third_party/swig/Lib": None,
    "src/third_party/usrsctp/usrsctplib": None,
    "src/third_party/v8-i18n": None,
    "src/third_party/webdriver/pylib": None,
    "src/third_party/webgl": None,
    "src/third_party/webpagereplay": None,
    "src/third_party/WebKit/LayoutTests/w3c/web-platform-tests": None,
    "src/third_party/WebKit/LayoutTests/w3c/csswg-test": None,
    "src/third_party/yasm/source/patched-yasm": None,
    "src/tools/page_cycler/acid3": None,
    "src/v8": None,
  },
  "mac": {
    "src/chrome/tools/test/reference_build/chrome_mac":
      "/trunk/deps/reference_builds/chrome_mac@273882",

    "src/third_party/google_toolbox_for_mac/src":
      (Var("googlecode_url") % "google-toolbox-for-mac") + "/trunk@" +
      Var("google_toolbox_for_mac_revision"),

    "src/third_party/pdfsqueeze":
      (Var("googlecode_url") % "pdfsqueeze") + "/trunk@5",

    "src/third_party/lighttpd":
      "/trunk/deps/third_party/lighttpd@" + Var("lighttpd_revision"),

    "src/third_party/swig/mac":
      "/trunk/deps/third_party/swig/mac@" + Var("swig_revision"),

    # NSS, for SSLClientSocketNSS.
    "src/third_party/nss":
      "/trunk/deps/third_party/nss@" + Var("nss_revision"),

    "src/chrome/installer/mac/third_party/xz/xz":
      "/trunk/deps/third_party/xz@233311",
  },
  "unix": {
    # Linux, really.
    "src/chrome/tools/test/reference_build/chrome_linux":
      "/trunk/deps/reference_builds/chrome_linux64@273882",

    "src/third_party/xdg-utils":
      "/trunk/deps/third_party/xdg-utils@203785",

    "src/third_party/swig/linux":
      "/trunk/deps/third_party/swig/linux@" + Var("swig_revision"),

    "src/third_party/lss":
      ((Var("googlecode_url") % "linux-syscall-support") + "/trunk/lss@" +
       Var("lss_revision")),

    # For Linux and Chromium OS.
    "src/third_party/cros_system_api":
      Var("chromiumos_git") + "/platform/system_api.git" +
      "@690c6b9702d7bcc761ae2d8976fe77e547255481",

    # Note that this is different from Android's freetype repo.
    "src/third_party/freetype2/src":
      Var("chromium_git") + "/chromium/src/third_party/freetype2.git" +
      "@d699c2994ecc178c4ed05ac2086061b2034c2178",

    # Build tools for Chrome OS.
    "src/third_party/chromite":
      Var("chromiumos_git") + "/chromite.git" +
      "@38242c6c77b77bcfb887fa97f3735c3262310370",

    # Dependency of chromite.git.
    "src/third_party/pyelftools":
      Var("chromiumos_git") + "/third_party/pyelftools.git" +
      "@bdc1d380acd88d4bfaf47265008091483b0d614e",

    "src/third_party/undoview":
      "/trunk/deps/third_party/undoview@119694",

    "src/third_party/liblouis/src":
      Var("chromium_git") +
      "/external/liblouis.git@3c2daee56250162e5a75830871601d74328d39f5",

    # Used for embedded builds. CrOS & Linux use the system version.
    "src/third_party/fontconfig/src":
      Var("chromium_git") + "/external/fontconfig.git" +
      "@f16c3118e25546c1b749f9823c51827a60aeb5c1",
  },
  "android": {
    "src/third_party/android_tools":
      Var("chromium_git") + "/android_tools.git" +
      "@8301b711a9ac7de56e9a9ff3dee0b2ebfc9a380f",

    "src/third_party/aosp":
      "/trunk/deps/third_party/aosp@148330",

    "src/third_party/apache-mime4j":
      "/trunk/deps/third_party/apache-mime4j@170888",

    "src/third_party/findbugs":
      "/trunk/deps/third_party/findbugs@245039",

    "src/third_party/freetype":
      Var("chromium_git") + "/chromium/src/third_party/freetype.git" +
      "@a2b9955b49034a51dfbc8bf9f4e9d312149cecac",

    "src/third_party/guava/src":
      Var("chromium_git") + "/external/guava-libraries.git" +
      "@c523556ab7d0f05afadebd20e7768d4c16af8771",

   "src/third_party/elfutils/src":
      Var("chromium_git") + "/external/elfutils.git" +
      "@249673729a7e5dbd5de4f3760bdcaa3d23d154d7",

    "src/third_party/httpcomponents-client":
      "/trunk/deps/third_party/httpcomponents-client@170888",

    "src/third_party/httpcomponents-core":
      "/trunk/deps/third_party/httpcomponents-core@170888",

    "src/third_party/jarjar":
      "/trunk/deps/third_party/jarjar@170888",

    "src/third_party/jsr-305/src":
      (Var("googlecode_url") % "jsr-305") + "/trunk@51",

    "src/third_party/lss":
      ((Var("googlecode_url") % "linux-syscall-support") + "/trunk/lss@" +
       Var("lss_revision")),

    "src/third_party/eyesfree/src/android/java/src/com/googlecode/eyesfree/braille":
      (Var("googlecode_url") % "eyes-free") + "/trunk/braille/client/src/com/googlecode/eyesfree/braille@797",
  },
}


include_rules = [
  # Everybody can use some things.
  "+base",
  "+build",
  "+ipc",

  # Everybody can use headers generated by tools/generate_library_loader.
  "+library_loaders",

  "+testing",
  "+third_party/icu/source/common/unicode",
  "+third_party/icu/source/i18n/unicode",
  "+url",
]


# checkdeps.py shouldn't check include paths for files in these dirs:
skip_child_includes = [
  "breakpad",
  "delegate_execute",
  "metro_driver",
  "native_client_sdk",
  "o3d",
  "sdch",
  "skia",
  "testing",
  "third_party",
  "v8",
  "win8",
]


hooks = [
  {
    # This downloads binaries for Native Client's newlib toolchain.
    # Done in lieu of building the toolchain from scratch as it can take
    # anywhere from 30 minutes to 4 hours depending on platform to build.
    "name": "nacltools",
    "pattern": ".",
    "action": [
        "python", "src/build/download_nacl_toolchains.py",
        "--exclude", "arm_trusted",
    ],
  },
  {
    # Downloads an ARM sysroot image to src/arm-sysroot. This image updates
    # at about the same rate that the chrome build deps change.
    # This script is a no-op except for linux users who have
    # target_arch=arm in their GYP_DEFINES.
    "name": "sysroot",
    "pattern": ".",
    "action": ["python", "src/build/linux/install-arm-sysroot.py",
               "--linux-only"],
  },
  {
    # Downloads the Debian Wheezy sysroot to chrome/installer/linux if needed.
    # This sysroot updates at about the same rate that the chrome build deps
    # change. This script is a no-op except for linux users who are doing
    # official chrome builds.
    "name": "sysroot",
    "pattern": ".",
    "action": [
        "python",
        "src/chrome/installer/linux/sysroot_scripts/install-debian.wheezy.sysroot.py",
        "--linux-only",
        "--arch=amd64"],
  },
  {
    # Same as above, but for 32-bit Linux.
    "name": "sysroot",
    "pattern": ".",
    "action": [
        "python",
        "src/chrome/installer/linux/sysroot_scripts/install-debian.wheezy.sysroot.py",
        "--linux-only",
        "--arch=i386"],
  },
  {
    # Pull clang if on Mac or clang is requested via GYP_DEFINES.
    "name": "clang",
    "pattern": ".",
    "action": ["python", "src/tools/clang/scripts/update.py", "--if-needed"],
  },
  {
    # Update the Windows toolchain if necessary.
    "name": "win_toolchain",
    "pattern": ".",
    "action": ["python", "src/build/vs_toolchain.py", "update"],
  },
  {
    # Update LASTCHANGE. This is also run by export_tarball.py in
    # src/tools/export_tarball - please keep them in sync.
    "name": "lastchange",
    "pattern": ".",
    "action": ["python", "src/build/util/lastchange.py",
               "-o", "src/build/util/LASTCHANGE"],
  },
  {
    # Update LASTCHANGE.blink. This is also run by export_tarball.py in
    # src/tools/export_tarball - please keep them in sync.
    "name": "lastchange",
    "pattern": ".",
    "action": ["python", "src/build/util/lastchange.py",
               "-s", "src/third_party/WebKit",
               "-o", "src/build/util/LASTCHANGE.blink"],
  },
  # Pull GN binaries. This needs to be before running GYP below.
  {
    "name": "gn_win",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=win32",
                "--no_auth",
                "--bucket", "chromium-gn",
                "-s", "src/buildtools/win/gn.exe.sha1",
    ],
  },
  {
    "name": "gn_mac",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=darwin",
                "--no_auth",
                "--bucket", "chromium-gn",
                "-s", "src/buildtools/mac/gn.sha1",
    ],
  },
  {
    "name": "gn_linux32",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=linux*",
                "--no_auth",
                "--bucket", "chromium-gn",
                "-s", "src/buildtools/linux32/gn.sha1",
    ],
  },
  {
    "name": "gn_linux64",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=linux*",
                "--no_auth",
                "--bucket", "chromium-gn",
                "-s", "src/buildtools/linux64/gn.sha1",
    ],
  },
  {
    # Remove GN binaries from tools/gn/bin that aren't used anymore.
    # TODO(brettw) remove after the end of July, 2014.
    "name": "remove_old_gn_binaries",
    "pattern": ".",
    "action": ["python", "src/tools/gn/bin/rm_binaries.py"],
  },
  # Pull clang-format binaries using checked-in hashes.
  {
    "name": "clang_format_win",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=win32",
                "--no_auth",
                "--bucket", "chromium-clang-format",
                "-s", "src/buildtools/win/clang-format.exe.sha1",
    ],
  },
  {
    "name": "clang_format_mac",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=darwin",
                "--no_auth",
                "--bucket", "chromium-clang-format",
                "-s", "src/buildtools/mac/clang-format.sha1",
    ],
  },
  {
    "name": "clang_format_linux",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=linux*",
                "--no_auth",
                "--bucket", "chromium-clang-format",
                "-s", "src/buildtools/linux64/clang-format.sha1",
    ],
  },
  # Pull binutils for linux, enabled debug fission for faster linking /
  # debugging when used with clang on Ubuntu Precise.
  # https://code.google.com/p/chromium/issues/detail?id=352046
  {
    "name": "binutils",
    "pattern": "src/third_party/binutils",
    "action": [
        "python",
        "src/third_party/binutils/download.py",
    ],
  },
  # Pull eu-strip binaries using checked-in hashes.
  {
    "name": "eu-strip",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=linux*",
                "--no_auth",
                "--bucket", "chromium-eu-strip",
                "-s", "src/build/linux/bin/eu-strip.sha1",
    ],
  },
  {
    "name": "drmemory",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=win32",
                "--no_auth",
                "--bucket", "chromium-drmemory",
                "-s", "src/third_party/drmemory/drmemory-windows-sfx.exe.sha1",
              ],
  },
  # Pull the Syzygy binaries, used for optimization and instrumentation.
  {
    "name": "syzygy-binaries",
    "pattern": ".",
    "action": ["python",
               "src/build/get_syzygy_binaries.py",
               "--output-dir=src/third_party/syzygy/binaries",
               "--revision=363bc02a09c380b6f5f397606cc0744d85d54a51",
               "--overwrite",
    ],
  },
  {
    "name": "apache_win32",
    "pattern": "\\.sha1",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=win32",
                "--directory",
                "--recursive",
                "--no_auth",
                "--num_threads=16",
                "--bucket", "chromium-apache-win32",
                "src/third_party/apache-win32",
    ],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "name": "gyp",
    "pattern": ".",
    "action": ["python", "src/build/gyp_chromium"],
  },
]
