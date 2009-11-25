deps = {
  "tools/gyp":
    "http://gyp.googlecode.com/svn/trunk@760",
  "build":
    "http://src.chromium.org/svn/trunk/src/build",
  "site_scons":
    "http://src.chromium.org/svn/trunk/src/site_scons",
  "testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@267",
  "third_party/binutils":
    "http://nativeclient.googlecode.com/svn/trunk/src/third_party/binutils@1005",
  "third_party/bison":
    "http://nativeclient.googlecode.com/svn/trunk/src/third_party/bison@1005",
  "third_party/flex":
    "http://nativeclient.googlecode.com/svn/trunk/src/third_party/flex@1005",
  "third_party/gcc":
    "http://nativeclient.googlecode.com/svn/trunk/src/third_party/gcc@1005",
  "third_party/gdb":
    "http://nativeclient.googlecode.com/svn/trunk/src/third_party/gdb@1005",
  "third_party/glibc":
    "http://nativeclient.googlecode.com/svn/trunk/src/third_party/glibc@1005",
  "third_party/gmp":
    "http://nativeclient.googlecode.com/svn/trunk/src/third_party/gmp@1005",
  "third_party/llvm":
    "http://nativeclient.googlecode.com/svn/trunk/src/third_party/llvm@1005",
  "third_party/mpfr":
    "http://nativeclient.googlecode.com/svn/trunk/src/third_party/mpfr@1005",
  "third_party/newlib":
    "http://nativeclient.googlecode.com/svn/trunk/src/third_party/newlib@1005",
  "third_party/scons":
    "http://nativeclient.googlecode.com/svn/trunk/src/third_party/scons@1005",
  "third_party/sdl":
    "http://nativeclient.googlecode.com/svn/trunk/src/third_party/sdl@1005",
  "third_party/zlib":
    "http://nativeclient.googlecode.com/svn/trunk/src/third_party/zlib@1005",
  "native_client_sdk":
    "http://nativeclient.googlecode.com/svn/trunk/src/native_client_sdk",
  # Includes needed for using Chromium NPAPI
  "third_party/npapi/bindings":
    "http://src.chromium.org/svn/trunk/src/third_party/npapi/bindings",
  "base":
    "http://src.chromium.org/svn/trunk/src/base",
  "chrome/common":
    "http://src.chromium.org/svn/trunk/src/chrome/common",
}

deps_os = {
  "win": {
    "third_party/python_24":
      "http://src.chromium.org/svn/trunk/deps/third_party/python_24@22967",
    "third_party/wix_2_0_4221":
      "http://o3d.googlecode.com/svn/trunk/googleclient/third_party/wix_2_0_4221@153",
# To be enabled after mingw-w64 is uploaded
#    "third_party/mingw-w64/mingw/bin":
#      "http://o3d.googlecode.com/svn/trunk/googleclient/third_party/mingw-w64/mingw/bin",
  },
}

hooks = [
  {
    "pattern": ".",
    "action": ["python", "native_client/build/gyp_nacl",
               "native_client/build/all.gyp"],
  },
]
