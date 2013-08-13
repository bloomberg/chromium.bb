# When adding a new dependency, please update the top-level .gitignore file
# to list the dependency's destination directory.

vars = {
  # Use this googlecode_url variable only if there is an internal mirror for it.
  # If you do not know, use the full path while defining your new deps entry.
  "googlecode_url": "http://%s.googlecode.com/svn",
  "sourceforge_url": "http://svn.code.sf.net/p/%(repo)s/code",
  "webkit_trunk": "http://src.chromium.org/blink/trunk",
  "nacl_trunk": "http://src.chromium.org/native_client/trunk",
  "webkit_revision": "155942",
  "chromium_git": "https://chromium.googlesource.com",
  "chromiumos_git": "https://chromium.googlesource.com/chromiumos",
  "skia_git": "https://skia.googlesource.com",
  "swig_revision": "69281",
  "nacl_revision": "11988",
  # After changing nacl_revision, run 'glient sync' and check native_client/DEPS
  # to update other nacl_*_revision's.
  "nacl_tools_revision": "11437",  # native_client/DEPS: tools_rev
  "gtm_revision": "616",

  "libphonenumber_revision": "584",
  "libvpx_revision": "212364",
  "lss_revision": "20",

  # These two FFmpeg variables must be updated together.  One is used for SVN
  # checkouts and the other for Git checkouts.
  "ffmpeg_revision": "214392",
  "ffmpeg_hash": "894e6f715645528e815aee2dad45b59704238dcd",

  "sfntly_revision": "134",
  "skia_revision": "10680",
  "skia_hash": "8f4825bfb1da69439e1e96a0c6ea04488757d95e",
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling Skia
  # and V8 without interference from each other.
  "v8_revision": "16159",
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling WebRTC
  # and V8 without interference from each other.
  "webrtc_revision": "4533",
  "jsoncpp_revision": "248",
  "nss_revision": "209026",
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling swarm_client
  # and whatever else without interference from each other.
  "swarm_revision": "217028",
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling openssl
  # and whatever else without interference from each other.
  "openssl_revision": "207965",
}

deps = {
  "src/breakpad/src":
    (Var("googlecode_url") % "google-breakpad") + "/trunk/src@1199",

  "src/sdch/open-vcdiff":
    (Var("googlecode_url") % "open-vcdiff") + "/trunk@42",

  "src/testing/gtest":
    (Var("googlecode_url") % "googletest") + "/trunk@629",

  "src/testing/gmock":
    (Var("googlecode_url") % "googlemock") + "/trunk@410",

  "src/third_party/angle_dx11":
    Var("chromium_git") +
    "/external/angle.git@b93f84acdecd3eebf316750d8024b8b592c9012b",

  "src/third_party/trace-viewer":
    (Var("googlecode_url") % "trace-viewer") + "/trunk@847",

  "src/third_party/WebKit":
    Var("webkit_trunk") + "@" + Var("webkit_revision"),

  "src/third_party/icu":
    "/trunk/deps/third_party/icu46@214189",

  "src/third_party/libexif/sources":
    "/trunk/deps/third_party/libexif/sources@146817",

  "src/third_party/hunspell":
   "/trunk/deps/third_party/hunspell@206172",

  "src/third_party/hunspell_dictionaries":
    "/trunk/deps/third_party/hunspell_dictionaries@193701",

  "src/third_party/safe_browsing/testing":
    (Var("googlecode_url") % "google-safe-browsing") + "/trunk/testing@112",

  "src/third_party/cacheinvalidation/src":
    (Var("googlecode_url") % "google-cache-invalidation-api") +
    "/trunk/src@309",

  "src/third_party/leveldatabase/src":
    (Var("googlecode_url") % "leveldb") + "/trunk@75",

  "src/third_party/snappy/src":
    (Var("googlecode_url") % "snappy") + "/trunk@74",

  "src/tools/grit":
    (Var("googlecode_url") % "grit-i18n") + "/trunk@129",

  "src/tools/gyp":
    (Var("googlecode_url") % "gyp") + "/trunk@1693",

  "src/tools/swarm_client":
    "/trunk/tools/swarm_client@" + Var("swarm_revision"),

  "src/v8":
    (Var("googlecode_url") % "v8") + "/trunk@" + Var("v8_revision"),

  "src/native_client":
    Var("nacl_trunk") + "/src/native_client@" + Var("nacl_revision"),

  "src/chrome/test/data/extensions/api_test/permissions/nacl_enabled/bin":
    Var("nacl_trunk") + "/src/native_client/tests/prebuilt@" +
    Var("nacl_revision"),

  "src/third_party/sfntly/cpp/src":
    (Var("googlecode_url") % "sfntly") + "/trunk/cpp/src@" +
    Var("sfntly_revision"),

  "src/third_party/skia/src":
    (Var("googlecode_url") % "skia") + "/trunk/src@" + Var("skia_revision"),

  "src/third_party/skia/gyp":
    (Var("googlecode_url") % "skia") + "/trunk/gyp@" + Var("skia_revision"),

  "src/third_party/skia/include":
    (Var("googlecode_url") % "skia") + "/trunk/include@" + Var("skia_revision"),

  "src/third_party/ots":
    (Var("googlecode_url") % "ots") + "/trunk@102",

  "src/tools/page_cycler/acid3":
    "/trunk/deps/page_cycler/acid3@171600",

  "src/chrome/test/data/perf/canvas_bench":
    "/trunk/deps/canvas_bench@122605",

  "src/chrome/test/data/perf/frame_rate/content":
    "/trunk/deps/frame_rate/content@93671",

  "src/chrome/test/data/perf/third_party/octane":
    (Var("googlecode_url") % "octane-benchmark") + "/trunk@19",

  "src/third_party/bidichecker":
    (Var("googlecode_url") % "bidichecker") + "/trunk/lib@4",

  # When roll to another webgl conformance tests revision, please goto
  # chrome/test/gpu and run generate_webgl_conformance_test_list.py.
  "src/third_party/webgl_conformance":
    "/trunk/deps/third_party/webgl/sdk/tests@217097",

  "src/third_party/swig/Lib":
    "/trunk/deps/third_party/swig/Lib@" + Var("swig_revision"),

  # Make sure you update the two functional.DEPS and webdriver.DEPS too.
  "src/third_party/webdriver/pylib":
    (Var("googlecode_url") % "selenium") + "/trunk/py@18337",

  "src/third_party/libvpx":
    "/trunk/deps/third_party/libvpx@" +
    Var("libvpx_revision"),

  "src/third_party/ffmpeg":
    "/trunk/deps/third_party/ffmpeg@" +
    Var("ffmpeg_revision"),

  "src/third_party/libjingle/source/talk":
    (Var("googlecode_url") % "webrtc") + "/stable/talk@" +
    Var("webrtc_revision"),

  "src/third_party/usrsctp/usrsctplib":
    (Var("googlecode_url") % "sctp-refimpl") +
    "/trunk/KERN/usrsctp/usrsctplib@8554",

  "src/third_party/libsrtp":
    "/trunk/deps/third_party/libsrtp@214783",

  "src/third_party/speex":
    "/trunk/deps/third_party/speex@198168",

  "src/third_party/yasm/source/patched-yasm":
    "/trunk/deps/third_party/yasm/patched-yasm@167605",

  "src/third_party/libjpeg_turbo":
    "/trunk/deps/third_party/libjpeg_turbo@177737",

  "src/third_party/flac":
    "/trunk/deps/third_party/flac@198139",

  "src/third_party/pyftpdlib/src":
    (Var("googlecode_url") % "pyftpdlib") + "/trunk@977",

  "src/third_party/scons-2.0.1":
    Var("nacl_trunk") + "/src/third_party/scons-2.0.1@" +
        Var("nacl_tools_revision"),

  "src/third_party/webrtc":
    (Var("googlecode_url") % "webrtc") + "/stable/webrtc@" + Var("webrtc_revision"),

  "src/third_party/openmax_dl":
    (Var("googlecode_url") % "webrtc") + "/deps/third_party/openmax@4261",

  "src/third_party/jsoncpp/source/include":
    (Var("sourceforge_url") % {"repo": "jsoncpp"}) +
        "/trunk/jsoncpp/include@" + Var("jsoncpp_revision"),

  "src/third_party/jsoncpp/source/src/lib_json":
    (Var("sourceforge_url") % {"repo": "jsoncpp"}) +
        "/trunk/jsoncpp/src/lib_json@" + Var("jsoncpp_revision"),

  "src/third_party/libyuv":
    (Var("googlecode_url") % "libyuv") + "/trunk@723",

  "src/third_party/smhasher/src":
    (Var("googlecode_url") % "smhasher") + "/trunk@149",

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
    "/trunk/tools/deps2git@214390",

  "src/third_party/webpagereplay":
    (Var("googlecode_url") % "web-page-replay") + "/trunk@518",

  "src/third_party/pywebsocket/src":
    (Var("googlecode_url") % "pywebsocket") + "/trunk/src@662",

  "src/third_party/opus/src":
    "/trunk/deps/third_party/opus@185324",

  "src/third_party/accessibility-developer-tools":
    Var("chromium_git") + "/external/accessibility-developer-tools.git@2e3c9c8e49277a0ca1eeba510271eb03f9486d8f",

  "src/media/cdm/ppapi/api":
    "/trunk/deps/cdm@181763",

  "src/third_party/mesa/src":
    "/trunk/deps/third_party/mesa@210110",
}


deps_os = {
  "win": {
    "src/chrome/tools/test/reference_build/chrome_win":
      "/trunk/deps/reference_builds/chrome_win@197743",

    "src/third_party/cygwin":
      "/trunk/deps/third_party/cygwin@133786",

    "src/third_party/python_26":
      "/trunk/tools/third_party/python_26@89111",

    "src/third_party/psyco_win32":
      "/trunk/deps/third_party/psyco_win32@79861",

    "src/third_party/bison":
      "/trunk/deps/third_party/bison@147303",

    "src/third_party/gperf":
      "/trunk/deps/third_party/gperf@147304",

    "src/third_party/perl":
      "/trunk/deps/third_party/perl@147900",

    "src/third_party/lighttpd":
      "/trunk/deps/third_party/lighttpd@33727",

    # Chrome Frame related deps
    "src/third_party/xulrunner-sdk":
      "/trunk/deps/third_party/xulrunner-sdk@119756",
    "src/chrome_frame/tools/test/reference_build/chrome_win":
      "/trunk/deps/reference_builds/chrome_win@89574",

    # Parses Windows PE/COFF executable format.
    "src/third_party/pefile":
      (Var("googlecode_url") % "pefile") + "/trunk@63",

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

    # Binary level profile guided optimizations. This points to the
    # latest release binaries for the toolchain.
    "src/third_party/syzygy/binaries":
      (Var("googlecode_url") % "sawbuck") + "/trunk/syzygy/binaries@1718",

    # Binaries for nacl sdk.
    "src/third_party/nacl_sdk_binaries":
      "/trunk/deps/third_party/nacl_sdk_binaries@111576",
  },
  "ios": {
    "src/third_party/GTM":
      (Var("googlecode_url") % "google-toolbox-for-mac") + "/trunk@" +
      Var("gtm_revision"),

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
    "src/native_client": None,
    "src/native_client/src/third_party/ppapi": None,
    "src/third_party/angle": None,
    "src/third_party/angle_dx11": None,
    "src/third_party/bidichecker": None,
    "src/third_party/webgl_conformance": None,
    "src/third_party/ffmpeg": None,
    "src/third_party/hunspell_dictionaries": None,
    "src/third_party/hunspell": None,
    "src/third_party/leveldatabase/src": None,
    "src/third_party/libexif/sources": None,
    "src/third_party/libjpeg_turbo": None,
    "src/third_party/libphonenumber/src/phonenumbers": None,
    "src/third_party/libphonenumber/src/test": None,
    "src/third_party/libphonenumber/src/resources": None,
    "src/third_party/libsrtp": None,
    "src/third_party/usrsctp/usrsctplib": None,
    "src/third_party/libvpx": None,
    "src/third_party/libyuv": None,
    "src/third_party/ots": None,
    "src/third_party/pylib": None,
    "src/third_party/pymox/src": None,
    "src/third_party/safe_browsing/testing": None,
    "src/third_party/scons-2.0.1": None,
    "src/third_party/sfntly/cpp/src": None,
    "src/third_party/smhasher/src": None,
    "src/third_party/snappy/src": None,
    "src/third_party/swig/Lib": None,
    "src/third_party/undoview": None,
    "src/third_party/v8-i18n": None,
    "src/third_party/webdriver/pylib": None,
    "src/third_party/webpagereplay": None,
    "src/third_party/webrtc": None,
    "src/third_party/yasm/source/patched-yasm": None,
    "src/tools/page_cycler/acid3": None,
    "src/v8": None,
  },
  "mac": {
    "src/chrome/tools/test/reference_build/chrome_mac":
      "/trunk/deps/reference_builds/chrome_mac@197743",

    "src/third_party/GTM":
      (Var("googlecode_url") % "google-toolbox-for-mac") + "/trunk@" +
      Var("gtm_revision"),
    "src/third_party/pdfsqueeze":
      (Var("googlecode_url") % "pdfsqueeze") + "/trunk@5",
    "src/third_party/lighttpd":
      "/trunk/deps/third_party/lighttpd@33737",

    "src/third_party/swig/mac":
      "/trunk/deps/third_party/swig/mac@" + Var("swig_revision"),

    # NSS, for SSLClientSocketNSS.
    "src/third_party/nss":
      "/trunk/deps/third_party/nss@" + Var("nss_revision"),

    "src/chrome/installer/mac/third_party/xz/xz":
      "/trunk/deps/third_party/xz@189101",
  },
  "unix": {
    # Linux, really.
    "src/chrome/tools/test/reference_build/chrome_linux":
      "/trunk/deps/reference_builds/chrome_linux@197743",

    "src/third_party/xdg-utils":
      "/trunk/deps/third_party/xdg-utils@203785",

    "src/third_party/swig/linux":
      "/trunk/deps/third_party/swig/linux@" + Var("swig_revision"),

    "src/third_party/lss":
      ((Var("googlecode_url") % "linux-syscall-support") + "/trunk/lss@" +
       Var("lss_revision")),

    "src/third_party/openssl":
      "/trunk/deps/third_party/openssl@" + Var("openssl_revision"),

    "src/third_party/gold":
      "/trunk/deps/third_party/gold@203758",

    "src/third_party/libmtp":
      "/trunk/deps/third_party/libmtp@206535",

    # Used on Linux only. CrOS already has a copy.
    "src/third_party/mtpd/source":
      Var("chromiumos_git") + "/platform/mtpd.git" +
      "@5be739c938a0a229ba9479b00b180e1f9c843e81",

    # Used on Linux only. CrOS already has a copy.
    "src/third_party/cros_dbus_cplusplus/source":
      Var("chromiumos_git") + "/third_party/dbus-cplusplus.git" +
      "@5e8f6d9db5c2abfb91d91f751184f25bb5cd0900",

    # For Linux and Chromium OS.
    "src/third_party/cros_system_api":
      Var("chromiumos_git") + "/platform/system_api.git" +
      "@9bad33f702cd5cefb0d5603d6853b26428425081",

    # Note that this is different from Android's freetype repo.
    "src/third_party/freetype2/src":
      Var("chromium_git") + "/chromium/src/third_party/freetype2.git" +
      "@d699c2994ecc178c4ed05ac2086061b2034c2178",

    # Build tools for targeting ChromeOS.
    "src/third_party/chromite":
      Var("chromiumos_git") + "/chromite.git" +
      "@214fb6506eda13715edd74087adf367a29809a3c",

    # Dependency of chromite.git.
    "src/third_party/pyelftools":
      Var("chromiumos_git") + "/third_party/pyelftools.git" +
      "@bdc1d380acd88d4bfaf47265008091483b0d614e",

    "src/third_party/undoview":
      "/trunk/deps/third_party/undoview@119694",
  },
  "android": {
    "src/third_party/android_tools":
      Var("chromium_git") + "/android_tools.git" +
      "@ceb64e7621d20573de3c566488ccf87241adc120",

    "src/third_party/aosp":
      "/trunk/deps/third_party/aosp@148330",

    "src/third_party/apache-mime4j":
      "/trunk/deps/third_party/apache-mime4j@170888",

    "src/third_party/findbugs":
      "/trunk/deps/third_party/findbugs@163586",

    "src/third_party/freetype":
      Var("chromium_git") + "/chromium/src/third_party/freetype.git" +
      "@96551feab72aac26836e9aaf4fae0962d39d5ab0",

    "src/third_party/guava/src":
      Var("chromium_git") + "/external/guava-libraries.git" +
      "@c523556ab7d0f05afadebd20e7768d4c16af8771",

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

    "src/third_party/openssl":
      "/trunk/deps/third_party/openssl@" + Var("openssl_revision"),

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
  "chrome_frame",
  "delegate_execute",
  "metro_driver",
  "native_client_sdk",
  "o3d",
  "pdf",
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
         "--no-arm-trusted",
         "--keep",
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
    # Pull clang on mac. If nothing changed, or on non-mac platforms, this takes
    # zero seconds to run. If something changed, it downloads a prebuilt clang,
    # which takes ~20s, but clang speeds up builds by more than 20s.
    "name": "clang",
    "pattern": ".",
    "action": ["python", "src/tools/clang/scripts/update.py", "--mac-only"],
  },
  {
    # Update the cygwin mount on Windows.
    "name": "cygwin",
    "pattern": ".",
    "action": ["python", "src/build/win/setup_cygwin_mount.py", "--win-only"],
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
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "name": "gyp",
    "pattern": ".",
    "action": ["python", "src/build/gyp_chromium"],
  },
  {
    # Check for landmines (reasons to clobber the build).
    "name": "landmines",
    "pattern": ".",
    "action": ["python", "src/build/landmines.py"],
  },
]
