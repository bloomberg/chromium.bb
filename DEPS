vars = {
  "chromium_trunk": "http://src.chromium.org/svn/trunk",
  "native_client_trunk": "http://nativeclient.googlecode.com/svn/trunk",
  "o3d_trunk": "http://o3d.googlecode.com/svn/trunk",
  "gyp_rev": "760",
  "gtest_rev": "267",
  "tools_rev": "1108",
  "python_rev": "22967",
  "wix_rev": "153",
}

deps = {
  "tools/gyp":
    "http://gyp.googlecode.com/svn/trunk@" + Var("gyp_rev"),
  "build":
    Var("chromium_trunk") + "/src/build",
  "site_scons":
    Var("chromium_trunk") + "/src/site_scons",
  "testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@" + Var("gtest_rev"),
  "third_party/binutils":
    Var("native_client_trunk") + "/src/third_party/binutils@" + Var("tools_rev"),
  "third_party/bison":
    Var("native_client_trunk") + "/src/third_party/bison@" + Var("tools_rev"),
  "third_party/flex":
    Var("native_client_trunk") + "/src/third_party/flex@" + Var("tools_rev"),
  "third_party/gcc":
    Var("native_client_trunk") + "/src/third_party/gcc@" + Var("tools_rev"),
  "third_party/gdb":
    Var("native_client_trunk") + "/src/third_party/gdb@" + Var("tools_rev"),
  "third_party/glibc":
    Var("native_client_trunk") + "/src/third_party/glibc@" + Var("tools_rev"),
  "third_party/gmp":
    Var("native_client_trunk") + "/src/third_party/gmp@" + Var("tools_rev"),
  "third_party/llvm":
    Var("native_client_trunk") + "/src/third_party/llvm@" + Var("tools_rev"),
  "third_party/mpfr":
    Var("native_client_trunk") + "/src/third_party/mpfr@" + Var("tools_rev"),
  "third_party/newlib":
    Var("native_client_trunk") + "/src/third_party/newlib@" + Var("tools_rev"),
  "third_party/scons":
    Var("native_client_trunk") + "/src/third_party/scons@" + Var("tools_rev"),
  "third_party/sdl":
    Var("native_client_trunk") + "/src/third_party/sdl@" + Var("tools_rev"),
  "third_party/selenium":
    Var("native_client_trunk") + "/src/third_party/selenium@" + Var("tools_rev"),
  "third_party/zlib":
    Var("native_client_trunk") + "/src/third_party/zlib@" + Var("tools_rev"),
  "native_client_sdk":
    Var("native_client_trunk") + "/src/native_client_sdk",
  # Includes needed for using Chromium NPAPI
  "third_party/npapi/bindings":
    Var("chromium_trunk") + "/src/third_party/npapi/bindings",
  "base":
    Var("chromium_trunk") + "/src/base",
  "chrome/common":
    Var("chromium_trunk") + "/src/chrome/common",
}

deps_os = {
  "win": {
    "third_party/python_24":
      Var("chromium_trunk") + "/deps/third_party/python_24@" + Var("python_rev"),
    "third_party/wix_2_0_4221":
      Var("o3d_trunk") + "/googleclient/third_party/wix_2_0_4221@" + Var("wix_rev"),
# To be enabled after mingw-w64 is uploaded
#    "third_party/mingw-w64/mingw/bin":
#      Var("o3d_trunk") + "/googleclient/third_party/mingw-w64/mingw/bin",
  },
}

hooks = [
  {
    "pattern": ".",
    "action": ["python", "native_client/build/gyp_nacl",
               "native_client/build/all.gyp"],
  },
]
