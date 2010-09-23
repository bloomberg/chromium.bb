vars = {
  "googlecode_url": "http://%s.googlecode.com/svn",
  "webkit_trunk": "http://svn.webkit.org/repository/webkit/trunk",
  "nacl_trunk": "http://src.chromium.org/native_client/trunk",
  "webkit_revision": "68178",
  "ffmpeg_revision": "58574",
  "skia_revision": "599",
  "chromium_git": "http://src.chromium.org/git",
  "swig_revision": "40423",
  "nacl_revision": "3186",
  "libvpx_revision": "60261",
  "libjingle_revision": "29",
}

deps = {
  "src/breakpad/src":
    (Var("googlecode_url") % "google-breakpad") + "/trunk/src@697",

  "src/build/util/support":
    "/trunk/deps/support@20411",

  "src/googleurl":
    (Var("googlecode_url") % "google-url") + "/trunk@146",

  "src/seccompsandbox":
    "http://seccompsandbox.googlecode.com/svn/trunk@119",

  "src/sdch/open-vcdiff":
    (Var("googlecode_url") % "open-vcdiff") + "/trunk@28",

  "src/testing/gtest":
    (Var("googlecode_url") % "googletest") + "/trunk@435",

  "src/third_party/angle":
    (Var("googlecode_url") % "angleproject") + "/trunk@429",

  "src/third_party/WebKit":
    "/trunk/deps/third_party/WebKit@33467",

  "src/third_party/icu":
    "/trunk/deps/third_party/icu42@58904",

  "src/third_party/hunspell":
   "/trunk/deps/third_party/hunspell@51690",

  "src/third_party/hunspell_dictionaries":
   "/trunk/deps/third_party/hunspell_dictionaries@51875",

  "src/third_party/protobuf2/src":
    (Var("googlecode_url") % "protobuf") + "/trunk@327",

  "src/third_party/cacheinvalidation/files":
    (Var("googlecode_url") % "google-cache-invalidation-api") + "/trunk@35",

  "src/tools/gyp":
    (Var("googlecode_url") % "gyp") + "/trunk@840",

  "src/v8":
    (Var("googlecode_url") % "v8") + "/trunk@5447",

  "src/native_client":
    Var("nacl_trunk") + "/src/native_client@" + Var("nacl_revision"),
  "src/chrome/test/data/extensions/api_test/permissions/nacl_enabled/bin":
    Var("nacl_trunk") + "/src/native_client/tests/prebuilt@" +
    Var("nacl_revision"),

  "src/third_party/skia/src":
    (Var("googlecode_url") % "skia") + "/trunk/src@" + Var("skia_revision"),

  "src/third_party/skia/include":
    (Var("googlecode_url") % "skia") + "/trunk/include@" + Var("skia_revision"),

  "src/third_party/WebKit/LayoutTests":
    Var("webkit_trunk") + "/LayoutTests@" + Var("webkit_revision"),

  "src/third_party/WebKit/JavaScriptCore":
    Var("webkit_trunk") + "/JavaScriptCore@" + Var("webkit_revision"),

  "src/third_party/WebKit/WebCore":
    Var("webkit_trunk") + "/WebCore@" + Var("webkit_revision"),

  "src/third_party/WebKit/WebKit/chromium":
    Var("webkit_trunk") + "/WebKit/chromium@" + Var("webkit_revision"),

  "src/third_party/WebKit/WebKitTools/DumpRenderTree":
    Var("webkit_trunk") + "/WebKitTools/DumpRenderTree@" + Var("webkit_revision"),

  "src/third_party/WebKit/WebKitTools/Scripts":
    Var("webkit_trunk") + "/WebKitTools/Scripts@" + Var("webkit_revision"),

  "src/third_party/ots":
    (Var("googlecode_url") % "ots") + "/trunk@35",

  "src/tools/page_cycler/acid3":
    "/trunk/deps/page_cycler/acid3@19546",

  # We run these layout tests as UI tests.  Since many of the buildbots that
  # run layout tests do NOT have access to the LayoutTest directory, we need
  # to map them here.  In practice, these do not take up much space.
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
  "src/chrome/test/data/layout_tests/LayoutTests/platform/chromium/fast/workers":
    Var("webkit_trunk") + "/LayoutTests/platform/chromium/fast/workers@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-mac/fast/events":
    Var("webkit_trunk") + "/LayoutTests/platform/chromium-mac/fast/events@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-mac/http/tests/workers":
    Var("webkit_trunk") + "/LayoutTests/platform/chromium-mac/http/tests/workers@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/platform/chromium-mac/storage/domstorage":
    Var("webkit_trunk") + "/LayoutTests/platform/chromium-mac/storage/domstorage@" +
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
  "src/chrome/test/data/layout_tests/LayoutTests/websocket/tests/workers":
    Var("webkit_trunk") + "/LayoutTests/websocket/tests/workers@" +
    Var("webkit_revision"),
  "src/chrome/test/data/layout_tests/LayoutTests/media":
    Var("webkit_trunk") + "/LayoutTests/media@" +
    Var("webkit_revision"),

  "src/third_party/swig/Lib":
    "/trunk/deps/third_party/swig/Lib@" + Var("swig_revision"),

  "src/third_party/ffmpeg":
    "/trunk/deps/third_party/ffmpeg/source@" +
    Var("ffmpeg_revision"),

  "src/third_party/libvpx/include":
    "/trunk/deps/third_party/libvpx/include@" +
    Var("libvpx_revision"),
  "src/third_party/libvpx/lib":
    "/trunk/deps/third_party/libvpx/lib@" +
    Var("libvpx_revision"),

  "src/third_party/ppapi":
    (Var("googlecode_url") % "ppapi") + "/trunk@261",

  "src/third_party/libjingle/source":
    (Var("googlecode_url") % "libjingle") + "/branches/nextsnap@" +
    Var("libjingle_revision"),

  "src/third_party/speex":
    "/trunk/deps/third_party/speex@56515",

  "src/third_party/libsrtp/src":
    "/trunk/deps/third_party/libsrtp@60080",
}


deps_os = {
  "win": {
    "src/chrome/tools/test/reference_build/chrome":
      "/trunk/deps/reference_builds/chrome@41984",

    "src/third_party/cygwin":
      "/trunk/deps/third_party/cygwin@11984",

    "src/third_party/python_24":
      "/trunk/deps/third_party/python_24@22967",

    "src/third_party/lighttpd":
      "/trunk/deps/third_party/lighttpd@33727",

    "src/third_party/ffmpeg/binaries/chromium/win/ia32":
      "/trunk/deps/third_party/ffmpeg/binaries/win@" + Var("ffmpeg_revision"),

    # Chrome Frame related deps
    "src/third_party/xulrunner-sdk":
      "/trunk/deps/third_party/xulrunner-sdk@17887",
    "src/chrome_frame/tools/test/reference_build/chrome":
      "/trunk/deps/reference_builds/chrome@51194",

    # Parses Windows PE/COFF executable format.
    "src/third_party/pefile":
      (Var("googlecode_url") % "pefile") + "/trunk@63",

    # NSS, for SSLClientSocketNSS.
    "src/third_party/nss":
      "/trunk/deps/third_party/nss@55513",

    "src/third_party/swig/win":
      "/trunk/deps/third_party/swig/win@" + Var("swig_revision"),

    "src/third_party/mingw-w64/mingw/bin":
      (Var("nacl_trunk") + "/src/third_party/mingw-w64/mingw/bin@" +
       Var("nacl_revision")),

    "src/rlz":
      (Var("googlecode_url") % "rlz") + "/trunk@23",
  },
  "mac": {
    "src/chrome/tools/test/reference_build/chrome_mac":
      "/trunk/deps/reference_builds/chrome_mac@41963",

    "src/third_party/GTM":
      (Var("googlecode_url") % "google-toolbox-for-mac") + "/trunk@375",
    "src/third_party/pdfsqueeze":
      (Var("googlecode_url") % "pdfsqueeze") + "/trunk@2",
    "src/third_party/lighttpd":
      "/trunk/deps/third_party/lighttpd@33737",

    "src/third_party/WebKit/WebKit/mac":
      Var("webkit_trunk") + "/WebKit/mac@" + Var("webkit_revision"),
    "src/third_party/WebKit/WebKitLibraries":
      Var("webkit_trunk") + "/WebKitLibraries@" + Var("webkit_revision"),

    # TODO(ajwong): Move this into the OS-independent block.
    "src/third_party/yasm/source/patched-yasm":
      "/trunk/deps/third_party/yasm/patched-yasm@50870",

    "src/third_party/swig/mac":
      "/trunk/deps/third_party/swig/mac@" + Var("swig_revision"),

    # NSS, for SSLClientSocketNSS.
    "src/third_party/nss":
      "/trunk/deps/third_party/nss@55513",

    "src/chrome/installer/mac/third_party/xz/xz":
      "/trunk/deps/third_party/xz@50504",
  },
  "unix": {
    # Linux, really.
    "src/chrome/tools/test/reference_build/chrome_linux":
      "/trunk/deps/reference_builds/chrome_linux@41515",

    "src/third_party/xdg-utils":
      "/trunk/deps/third_party/xdg-utils@58316",

    # TODO(ajwong): Move this into the OS-independent block.
    "src/third_party/yasm/source/patched-yasm":
      "/trunk/deps/third_party/yasm/patched-yasm@50870",

    "src/third_party/swig/linux":
      "/trunk/deps/third_party/swig/linux@" + Var("swig_revision"),
  },
}


include_rules = [
  # Everybody can use some things.
  "+base",
  "+build",
  "+ipc",
  "+rlz",

  # For now, we allow ICU to be included by specifying "unicode/...", although
  # this should probably change.
  "+unicode",
  "+testing",
]


# checkdeps.py shouldn't check include paths for files in these dirs:
skip_child_includes = [
   "breakpad",
   "chrome_frame",
   "gears",
   "native_client",
   "o3d",
   "sdch",
   "skia",
   "testing",
   "third_party",
   "v8",
]


hooks = [
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "src/build/gyp_chromium"],
  },
]
