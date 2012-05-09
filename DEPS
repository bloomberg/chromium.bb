# DEPS files look like -*- Python -*-

vars = {
  # These two should be updated to the same revision whenever possible.
  "x86_toolchain_version": "8524",
  "pnacl_toolchain_version": "8524",

  # ARM trusted toolchain version cannot be changed for now.
  "arm_trusted_toolchain_version": "7004",

  # When possible, keep chrome_rev and chromebinaries_rev at the same
  # value.  chromebinaries_rev should match LKGR so that we can download
  # the binaries, but sometimes chrome_rev needs to be more recent to get
  # source changes when LKGR is lagging too far back.
  "chromebinaries_rev": "134867",
  "chrome_rev": "134867",
  # NOTE!  These four should match their counterparts in chromium/src/DEPS.
  # Be sure to update them when updating chrome_rev, above.
  "gtest_rev": "560",
  "gyp_rev": "1246",
  "jsoncpp_revision": "248",
  "lss_rev": "9",

  "lcov_rev": "54822",
  "python_26_rev": "53573",
  "tools_rev": "8228",

  # These are URL prefixes rather than revision numbers, so keep them separate.
  "chromium_trunk": "http://src.chromium.org/svn/trunk",
  "googlecode_url": "http://%s.googlecode.com/svn",
  "native_client_trunk": "http://src.chromium.org/native_client/trunk",
  "o3d_trunk": "http://o3d.googlecode.com/svn/trunk",
  "sourceforge_url": "http://%(repo)s.svn.sourceforge.net/svnroot/%(repo)s",
}

deps = {
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
  "validator_snapshots":
    Var("native_client_trunk") + "/src/validator_snapshots@" + Var("tools_rev"),
  "third_party/asan":
    Var("chromium_trunk") + "/deps/third_party/asan@" + Var("chrome_rev"),
  "third_party/gles2_book":
    Var("chromium_trunk") + "/src/third_party/gles2_book@" + Var("chrome_rev"),
  "third_party/jsoncpp":
    Var("chromium_trunk") + "/src/third_party/jsoncpp@" + Var("chrome_rev"),
  # NOTE! These two for jsoncpp are taken verbatim from chromium/src/DEPS
  # and must match what is used there.
  "third_party/jsoncpp/source/include":
    (Var("sourceforge_url") % {"repo": "jsoncpp"}) +
        "/trunk/jsoncpp/include@" + Var("jsoncpp_revision"),
  "third_party/jsoncpp/source/src/lib_json":
    (Var("sourceforge_url") % {"repo": "jsoncpp"}) +
        "/trunk/jsoncpp/src/lib_json@" + Var("jsoncpp_revision"),
  "third_party/lcov":
    Var("chromium_trunk") + "/src/third_party/lcov@" + Var("lcov_rev"),
  "third_party/lss":
    ((Var("googlecode_url") % "linux-syscall-support") +
     "/trunk/lss@" + Var("lss_rev")),
  "third_party/khronos":
    Var("chromium_trunk") + "/src/third_party/khronos@" + Var("chrome_rev"),
  "third_party/ply":
    Var("chromium_trunk") + "/src/third_party/ply@" + Var("chrome_rev"),
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
    # GNU binutils assembler for x86-32.
    "third_party/gnu_binutils":
      (Var("native_client_trunk") + "/deps/third_party/gnu_binutils@" +
       Var("tools_rev")),
    # GNU binutils assembler for x86-64.
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
               "--pnacl-version", Var("pnacl_toolchain_version"),
               "--keep"],
  },
  {
    "pattern": ".",
    "action": ["python", "native_client/build/gyp_nacl"],
  },
]
