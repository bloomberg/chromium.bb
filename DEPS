# DEPS files look like -*- Python -*-

vars = {
  # These two should be updated to the same revision whenever possible.
  "x86_toolchain_version": "7098",
  "pnacl_toolchain_version": "7098",

  # ARM trusted toolchain version cannot be changed for now.
  "arm_trusted_toolchain_version": "6645",

  # When possible, keep chrome_rev and chromebinaries_rev at the same
  # value.  chromebinaries_rev should match LKGR so that we can download
  # the binaries, but sometimes chrome_rev needs to be more recent to get
  # source changes when LKGR is lagging too far back.
  "chromebinaries_rev": "107928",
  "chrome_rev": "107928",
  "chromium_trunk": "http://src.chromium.org/svn/trunk",
  "googlecode_url": "http://%s.googlecode.com/svn",
  "gtest_rev": "560",
  "gyp_rev": "1083",
  "lcov_rev": "54822",
  "native_client_trunk": "http://src.chromium.org/native_client/trunk",
  "o3d_trunk": "http://o3d.googlecode.com/svn/trunk",
  "python_26_rev": "53573",
  "tools_rev": "7088",
  "wix_rev": "153",
}

deps = {
  # This gets us Chromium's DEPS file as ../DEPS, which is where it
  # needs to be when we run ../tools/clang/scripts/update.sh and it
  # wants to grep clang_revision out of it.
  ".":
    File(Var("chromium_trunk") + "/src/DEPS@" + Var("chrome_rev")),
  "base":
    Var("chromium_trunk") + "/src/base@" + Var("chrome_rev"),
  "build":
    Var("chromium_trunk") + "/src/build@" + Var("chrome_rev"),
  "chrome/test/pyautolib":
    Var("chromium_trunk") + "/src/chrome/test/pyautolib@" + Var("chrome_rev"),
  "gpu":
    Var("chromium_trunk") + "/src/gpu@" + Var("chrome_rev"),
  "native_client/tools/perf_expectations":
    Var("chromium_trunk") + "/src/tools/perf_expectations@" + Var("chrome_rev"),
  "net/tools/testserver":
    Var("chromium_trunk") + "/src/net/tools/testserver@" + Var("chrome_rev"),
  "ppapi":
    Var("chromium_trunk") + "/src/ppapi@" + Var("chrome_rev"),
  "testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@" + Var("gtest_rev"),
  "third_party":
    Var("native_client_trunk") + "/src/third_party@" + Var("tools_rev"),
  "third_party/asan":
    Var("chromium_trunk") + "/deps/third_party/asan@" + Var("chrome_rev"),
  "third_party/gles2_book":
    Var("chromium_trunk") + "/src/third_party/gles2_book@" + Var("chrome_rev"),
  "third_party/lcov":
    Var("chromium_trunk") + "/src/third_party/lcov@" + Var("lcov_rev"),
  "third_party/pyftpdlib":
    Var("chromium_trunk") + "/src/third_party/pyftpdlib@" + Var("chrome_rev"),
  "third_party/pyftpdlib/src":
    (Var("googlecode_url") % "pyftpdlib") + "/trunk@888",
  "third_party/simplejson":
    (Var("chromium_trunk") + "/tools/build/third_party/simplejson@" +
     Var("chrome_rev")),
  "third_party/tlslite":
    Var("chromium_trunk") + "/src/third_party/tlslite@" + Var("chrome_rev"),
  "tools/clang":
    Var("chromium_trunk") + "/src/tools/clang@" + Var("chrome_rev"),
  "tools/gyp":
    "http://gyp.googlecode.com/svn/trunk@" + Var("gyp_rev"),
  "tools/valgrind":
    Var("chromium_trunk") + "/src/tools/valgrind@" + Var("chrome_rev"),
}

deps_os = {
  "win": {
    "third_party/mingw-w64/mingw/bin":
      (Var("native_client_trunk") + "/deps/third_party/mingw-w64/mingw/bin@" +
       Var("tools_rev")),
    "third_party/NSIS":
      (Var("native_client_trunk") + "/deps/third_party/NSIS@" +
       Var("tools_rev")),
    "third_party/python_26":
      (Var("chromium_trunk") + "/tools/third_party/python_26@" +
       Var("python_26_rev")),
  },
}

hooks = [
  {
    "pattern": ".",
    "action": ["python", "native_client/build/download_toolchains.py",
               "--x86-version", Var("x86_toolchain_version"),
               "--arm-trusted-version", Var("arm_trusted_toolchain_version"),
               "--pnacl-version", Var("pnacl_toolchain_version")],
  },
  {
    "pattern": ".",
    "action": ["python", "native_client/build/gyp_nacl"],
  },
]
