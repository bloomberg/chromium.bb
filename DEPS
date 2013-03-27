# DEPS files look like -*- Python -*-

vars = {
  "chrome_rev": "191009",
  # NOTE!  These three should match their counterparts in chromium/src/DEPS.
  # Be sure to update them when updating chrome_rev, above.
  "gtest_rev": "629",
  "gyp_rev": "1603",
  "lss_rev": "18",

  "lcov_rev": "149720",
  "python_26_rev": "53573",
  "tools_rev": "11040",

  # These are URL prefixes rather than revision numbers, so keep them separate.
  "chromium_trunk": "http://src.chromium.org/svn/trunk",
  "googlecode_url": "http://%s.googlecode.com/svn",
  "native_client_trunk": "http://src.chromium.org/native_client/trunk",
}

deps = {
  "native_client/tools/perf_expectations":
    Var("chromium_trunk") + "/src/tools/perf_expectations@" + Var("chrome_rev"),
  "testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@" + Var("gtest_rev"),
  "third_party":
    Var("native_client_trunk") + "/src/third_party@" + Var("tools_rev"),
  "validator_snapshots":
    Var("native_client_trunk") + "/src/validator_snapshots@" + Var("tools_rev"),
  "third_party/asan":
    Var("chromium_trunk") + "/deps/third_party/asan@" + Var("chrome_rev"),
  "third_party/lcov":
    Var("chromium_trunk") + "/src/third_party/lcov@" + Var("lcov_rev"),
  "third_party/lss":
    ((Var("googlecode_url") % "linux-syscall-support") +
     "/trunk/lss@" + Var("lss_rev")),
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
  "unix": {
    # Ragel for validator_ragel
    "third_party/ragel":
      (Var("native_client_trunk") + "/deps/third_party/ragel@" +
       Var("tools_rev")),
  }
}

hooks = [
  {
    "pattern": ".",
    "action": ["python", "native_client/build/download_toolchains.py",
               "--keep", "--arm-untrusted", "native_client/TOOL_REVISIONS"],
  },
  {
    "pattern": ".",
    "action": ["python", "native_client/build/gyp_nacl"],
  },
]

include_rules = [
  "+native_client/src/include",
  "+gtest",
]
