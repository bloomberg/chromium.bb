vars = {
  # Use this googlecode_url variable only if there is an internal mirror for it.
  # If you do not know, use the full path while defining your new deps entry.
  "googlecode_url": "http://%s.googlecode.com/svn",
  "webkit_trunk": "http://svn.webkit.org/repository/webkit/trunk",
  "nacl_trunk": "http://src.chromium.org/native_client/trunk",
  "webkit_revision": "95596",
  "chromium_git": "http://git.chromium.org/git",
  "swig_revision": "69281",
  "nacl_revision": "6736",
  # After changing nacl_revision, run 'glient sync' and check native_client/DEPS
  # to update other nacl_*_revision's.
  # TODO(brettw) We should use the "From" syntax to avoid hardcoding the
  # revisions here, but it makes checkdeps confused. We should fix checkdeps.
  "nacl_tools_revision": "6504",  # native_client/DEPS: tools_rev

  # These hashes need to be updated when nacl_toolchain_revision is changed.
  # After changing nacl_toolchain_revision, run 'gclient runhooks' to get the
  # new values.
  "nacl_toolchain_mac_x86_newlib_hash":
      "1b0855435c03c435a011c6105a509624b2a4edaa",
  "nacl_toolchain_win_x86_newlib_hash":
      "5038a47b5a9a49acdc36cbe311aec7bce575c164",
  "nacl_toolchain_linux_x86_newlib_hash":
      "01e245dc6dca16bea5cf840dbc77e3aa138f234f",
  "nacl_toolchain_revision": "6494",

  "libjingle_revision": "85",
  "libvpx_revision": "97420",
  "ffmpeg_revision": "99115",
  "sfntly_revision": "54",
  "skia_revision": "2291",
  "v8_revision": "9288",
  "webrtc_revision": "634",

  # Note: On most bots, clang is not checked out via DEPS but by
  # tools/clang/scripts/update.sh. The script reads this line here.
  # Do NOT CHANGE this if you don't know what you're doing -- see
  # http://code.google.com/p/chromium/wiki/UpdatingClang
  "clang_revision": "139990",
}

deps = {
  "src/breakpad/src":
    (Var("googlecode_url") % "google-breakpad") + "/trunk/src@828",

  "src/build/util/support":
    "/trunk/deps/support@20411",

  "src/googleurl":
    (Var("googlecode_url") % "google-url") + "/trunk@159",

  "src/seccompsandbox":
    (Var("googlecode_url") % "seccompsandbox") + "/trunk@169",

  "src/sdch/open-vcdiff":
    (Var("googlecode_url") % "open-vcdiff") + "/trunk@28",

  "src/testing/gtest":
    (Var("googlecode_url") % "googletest") + "/trunk@560",

  "src/testing/gmock":
    (Var("googlecode_url") % "googlemock") + "/trunk@374",

  "src/third_party/angle":
    (Var("googlecode_url") % "angleproject") + "/trunk@754",

  # Note that this is *not* where we check out WebKit -- this just
  # puts some extra files into place for the real WebKit checkout to
  # happen.  See lines mentioning "webkit_revision" for the real
  # WebKit checkout.
  "src/third_party/WebKit":
    "/trunk/deps/third_party/WebKit@76115",

  "src/third_party/icu":
    "/trunk/deps/third_party/icu46@101184",

  "src/third_party/hunspell":
   "/trunk/deps/third_party/hunspell@96723",

  "src/third_party/hunspell_dictionaries":
    "/trunk/deps/third_party/hunspell_dictionaries@79099",

  "src/third_party/safe_browsing/testing":
    (Var("googlecode_url") % "google-safe-browsing") + "/trunk/testing@106",

  "src/third_party/cacheinvalidation/files/src/google":
    (Var("googlecode_url") % "google-cache-invalidation-api") +
    "/trunk/src/google@135",

  "src/third_party/leveldatabase/src":
    (Var("googlecode_url") % "leveldb") + "/trunk@47",

  "src/third_party/snappy/src":
    (Var("googlecode_url") % "snappy") + "/trunk@37",

  "src/tools/gyp":
    (Var("googlecode_url") % "gyp") + "/trunk@1050",

  "src/v8":
    (Var("googlecode_url") % "v8") + "/trunk@" + Var("v8_revision"),

  "src/native_client":
    Var("nacl_trunk") + "/src/native_client@" + Var("nacl_revision"),

  "src/chrome/test/data/extensions/api_test/permissions/nacl_enabled/bin":
    Var("nacl_trunk") + "/src/native_client/tests/prebuilt@" +
    Var("nacl_revision"),

  "src/third_party/sfntly/src/sfntly":
    (Var("googlecode_url") % "sfntly") + "/trunk/cpp/src/sfntly@" +
    Var("sfntly_revision"),

  "src/third_party/skia/gpu":
    (Var("googlecode_url") % "skia") + "/trunk/gpu@" + Var("skia_revision"),

  "src/third_party/skia/src":
    (Var("googlecode_url") % "skia") + "/trunk/src@" + Var("skia_revision"),

  "src/third_party/skia/include":
    (Var("googlecode_url") % "skia") + "/trunk/include@" + Var("skia_revision"),

  "src/third_party/skia/third_party/glu":
    (Var("googlecode_url") % "skia") + "/trunk/third_party/glu@" + Var("skia_revision"),

  "src/third_party/WebKit/LayoutTests":
    Var("webkit_trunk") + "/LayoutTests@" + Var("webkit_revision"),

  "src/third_party/WebKit/Source":
    Var("webkit_trunk") + "/Source@" + Var("webkit_revision"),

  "src/third_party/WebKit/Tools/DumpRenderTree":
    Var("webkit_trunk") + "/Tools/DumpRenderTree@" + Var("webkit_revision"),

  "src/third_party/WebKit/Tools/Scripts":
    Var("webkit_trunk") + "/Tools/Scripts@" + Var("webkit_revision"),

  "src/third_party/WebKit/Tools/TestWebKitAPI":
    Var("webkit_trunk") + "/Tools/TestWebKitAPI@" + Var("webkit_revision"),

  "src/third_party/ots":
    (Var("googlecode_url") % "ots") + "/trunk@69",

  "src/tools/page_cycler/acid3":
    "/trunk/deps/page_cycler/acid3@19546",

  "src/chrome/test/data/perf/frame_rate/content":
    "/trunk/deps/frame_rate/content@93671",

  "src/third_party/bidichecker":
    (Var("googlecode_url") % "bidichecker") + "/trunk/lib@4",

  "src/third_party/v8-i18n":
    (Var("googlecode_url") % "v8-i18n") + "/trunk@4",

  "src/third_party/webgl_conformance":
    "/trunk/deps/third_party/webgl/sdk/tests@93490",

  # We run these layout tests as UI tests. Since many of the buildbots that
  # run layout tests do NOT have access to the LayoutTest directory, we need
  # to map them here. In practice, these do not take up much space.
  "src/chrome/test/data/layout_tests/LayoutTests/fast/events":
    Var("webkit_trunk") + "/LayoutTests/fast/events@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/fast/js/resources":
    Var("webkit_trunk") + "/LayoutTests/fast/js/resources@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/fast/workers":
    Var("webkit_trunk") + "/LayoutTests/fast/workers@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/http/tests/resources":
    Var("webkit_trunk") + "/LayoutTests/http/tests/resources@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/http/tests/workers":
    Var("webkit_trunk") + "/LayoutTests/http/tests/workers@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/http/tests/xmlhttprequest":
    Var("webkit_trunk") + "/LayoutTests/http/tests/xmlhttprequest@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/http/tests/websocket/tests":
    Var("webkit_trunk") + "/LayoutTests/http/tests/websocket/tests@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/platform/chromium/fast/workers":
    Var("webkit_trunk") + "/LayoutTests/platform/chromium/fast/workers@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-cg-mac/fast/events":
    Var("webkit_trunk") + "/LayoutTests/platform/chromium-cg-mac/fast/events@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-cg-mac/http/tests/workers":
    Var("webkit_trunk") + "/LayoutTests/platform/chromium-cg-mac/http/tests/workers@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-cg-mac/storage/domstorage":
    Var("webkit_trunk") + "/LayoutTests/platform/chromium-cg-mac/storage/domstorage@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/fast/events":
    Var("webkit_trunk") + "/LayoutTests/platform/chromium-win/fast/events@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/fast/workers":
    Var("webkit_trunk") + "/LayoutTests/platform/chromium-win/fast/workers@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/http/tests/appcache":
    Var("webkit_trunk") + "/LayoutTests/http/tests/appcache@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/http/tests/workers":
    Var("webkit_trunk") + "/LayoutTests/platform/chromium-win/http/tests/workers@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-win/storage/domstorage":
    Var("webkit_trunk") + "/LayoutTests/platform/chromium-win/storage/domstorage@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/storage/domstorage":
    Var("webkit_trunk") + "/LayoutTests/storage/domstorage@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/media":
    Var("webkit_trunk") + "/LayoutTests/media@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/fast/filesystem/workers":
    Var("webkit_trunk") + "/LayoutTests/fast/filesystem/workers@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/fast/filesystem/resources":
    Var("webkit_trunk") + "/LayoutTests/fast/filesystem/resources@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/http/tests/filesystem":
    Var("webkit_trunk") + "/LayoutTests/http/tests/filesystem@" +
    Var("webkit_revision"),

  "src/third_party/swig/Lib":
    "/trunk/deps/third_party/swig/Lib@" + Var("swig_revision"),

  # WebDriver keeps client and test code separate, and merges when the
  # python egg is installed. We run tests directly from src, so import
  # the code into the structure expected by the tests.
  # Make sure you update chrome/test/functional.DEPS/DEPS too.
  "src/third_party/webdriver/python/selenium":
    "http://selenium.googlecode.com/svn/trunk/py/selenium@13487",
  "src/third_party/webdriver/python/selenium/test":
    "http://selenium.googlecode.com/svn/trunk/py/test@13487",

  "src/third_party/libvpx":
    "/trunk/deps/third_party/libvpx@" +
    Var("libvpx_revision"),

  "src/third_party/ffmpeg":
    "/trunk/deps/third_party/ffmpeg/source@" +
    Var("ffmpeg_revision"),

  "src/third_party/libjingle/source":
    (Var("googlecode_url") % "libjingle") + "/trunk@" +
    Var("libjingle_revision"),

  "src/third_party/speex":
    "/trunk/deps/third_party/speex@83190",

  "src/third_party/yasm/source/patched-yasm":
    "/trunk/deps/third_party/yasm/patched-yasm@73761",

  "src/third_party/libjpeg_turbo":
    "/trunk/deps/third_party/libjpeg_turbo@95800",

  "src/third_party/flac":
    "/trunk/deps/third_party/flac@96404",

  "src/third_party/pyftpdlib/src":
    (Var("googlecode_url") % "pyftpdlib") + "/trunk@888",

  # Needed to support nacl browser test jig.
  "src/third_party/pylib":
    # TODO(brettw) enable the From() line instead when checkdeps is fixed.
    #From("src/native_client", "native_client/src/third_party/pylib"),
    Var("nacl_trunk") + "/src/third_party/pylib@" + Var("nacl_tools_revision"),
  "src/third_party/scons-2.0.1":
    Var("nacl_trunk") + "/src/third_party/scons-2.0.1@" +
        Var("nacl_tools_revision"),

  "src/third_party/webrtc":
    (Var("googlecode_url") % "webrtc") + "/trunk/src@" + Var("webrtc_revision"),

  "src/third_party/mozc/session":
    (Var("googlecode_url") % "mozc") + "/trunk/src/session@58",

  "src/third_party/smhasher/src":
    (Var("googlecode_url") % "smhasher") + "/trunk@136",
}


deps_os = {
  "win": {
    "src/chrome/tools/test/reference_build/chrome_win":
      "/trunk/deps/reference_builds/chrome_win@89574",

    "src/third_party/cygwin":
      "/trunk/deps/third_party/cygwin@66844",

    "src/third_party/python_26":
      "/trunk/tools/third_party/python_26@89111",

    "src/third_party/psyco_win32":
      "/trunk/deps/third_party/psyco_win32@79861",

    "src/third_party/lighttpd":
      "/trunk/deps/third_party/lighttpd@33727",

    "src/third_party/ffmpeg/binaries/chromium/win/ia32":
      "/trunk/deps/third_party/ffmpeg/binaries/win@" + Var("ffmpeg_revision"),

    # Chrome Frame related deps
    "src/third_party/xulrunner-sdk":
      "/trunk/deps/third_party/xulrunner-sdk@71609",
    "src/chrome_frame/tools/test/reference_build/chrome_win":
      "/trunk/deps/reference_builds/chrome_win@89574",

    # Parses Windows PE/COFF executable format.
    "src/third_party/pefile":
      (Var("googlecode_url") % "pefile") + "/trunk@63",

    # NSS, for SSLClientSocketNSS.
    "src/third_party/nss":
      "/trunk/deps/third_party/nss@101569",

    "src/third_party/swig/win":
      "/trunk/deps/third_party/swig/win@" + Var("swig_revision"),

    "src/third_party/mingw-w64/mingw/bin":
      (Var("nacl_trunk") + "/src/third_party/mingw-w64/mingw/bin@" +
       Var("nacl_tools_revision")),

    "src/rlz":
      (Var("googlecode_url") % "rlz") + "/trunk@47",

    # Dependencies used by libjpeg-turbo
    "src/third_party/yasm/binaries":
      "/trunk/deps/third_party/yasm/binaries@74228",

    # Binary level profile guided optimizations.  This points to the
    # latest release binaries for the toolchain.
    "src/third_party/syzygy/binaries":
      (Var("googlecode_url") % "sawbuck") + "/trunk/syzygy/binaries@427",
  },
  "mac": {
    "src/chrome/tools/test/reference_build/chrome_mac":
      "/trunk/deps/reference_builds/chrome_mac@89574",

    "src/third_party/GTM":
      (Var("googlecode_url") % "google-toolbox-for-mac") + "/trunk@459",
    "src/third_party/pdfsqueeze":
      (Var("googlecode_url") % "pdfsqueeze") + "/trunk@4",
    "src/third_party/lighttpd":
      "/trunk/deps/third_party/lighttpd@33737",

    "src/third_party/WebKit/WebKitLibraries":
      Var("webkit_trunk") + "/WebKitLibraries@" + Var("webkit_revision"),

    "src/third_party/swig/mac":
      "/trunk/deps/third_party/swig/mac@" + Var("swig_revision"),

    # NSS, for SSLClientSocketNSS.
    "src/third_party/nss":
      "/trunk/deps/third_party/nss@101569",

    "src/chrome/installer/mac/third_party/xz/xz":
      "/trunk/deps/third_party/xz@87706",
  },
  "unix": {
    # Linux, really.
    "src/chrome/tools/test/reference_build/chrome_linux":
      "/trunk/deps/reference_builds/chrome_linux@89574",

    "src/third_party/xdg-utils":
      "/trunk/deps/third_party/xdg-utils@93299",

    "src/third_party/swig/linux":
      "/trunk/deps/third_party/swig/linux@" + Var("swig_revision"),

    "src/third_party/lss":
      (Var("googlecode_url") % "linux-syscall-support") + "/trunk/lss@3",

    "src/third_party/openssl":
      "/trunk/deps/third_party/openssl@101988",

    "src/third_party/WebKit/Tools/gdb":
      Var("webkit_trunk") + "/Tools/gdb@" + Var("webkit_revision"),
  },
}


include_rules = [
  # Everybody can use some things.
  "+base",
  "+build",
  "+ipc",

  # For now, we allow ICU to be included by specifying "unicode/...", although
  # this should probably change.
  "+unicode",
  "+testing",
]


# checkdeps.py shouldn't check include paths for files in these dirs:
skip_child_includes = [
  "breakpad",
  "chrome_frame",
  "native_client",
  "o3d",
  "pdf",
  "sdch",
  "skia",
  "testing",
  "third_party",
  "v8",
]


hooks = [
  {
    # This downloads binaries for Native Client's newlib toolchain.
    # Done in lieu of building the toolchain from scratch as it can take
    # anywhere from 30 minutes to 4 hours depending on platform to build.
    "pattern": ".",
    "action": [
        "python", "src/build/download_nacl_toolchains.py",
         "--x86-version", Var("nacl_toolchain_revision"),
         "--nacl-newlib-only",
         "--file-hash", "mac_x86_newlib",
             Var("nacl_toolchain_mac_x86_newlib_hash"),
         "--file-hash", "win_x86_newlib",
             Var("nacl_toolchain_win_x86_newlib_hash"),
         "--file-hash", "linux_x86_newlib",
             Var("nacl_toolchain_linux_x86_newlib_hash"),
    ],
  },
  {
    # Pull clang on mac. If nothing changed, or on non-mac platforms, this takes
    # zero seconds to run. If something changed, it downloads a prebuilt clang,
    # which takes ~20s, but clang speeds up builds by more than 20s.
    "pattern": ".",
    "action": ["python", "src/tools/clang/scripts/update.py", "--mac-only"],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "src/build/gyp_chromium"],
  },
]
