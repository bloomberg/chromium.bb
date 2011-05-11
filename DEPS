vars = {
  # These two should be updated to the same revision whenever possible.
  "arm_toolchain_version": "5262",
  "x86_toolchain_version": "5252",
  "chrome_rev": "84963",
  "chromium_trunk": "http://src.chromium.org/svn/trunk",
  "gtest_rev": "364",
  "gyp_rev": "906",
  "lcov_rev": "54822",
  "native_client_trunk": "http://src.chromium.org/native_client/trunk",
  "o3d_trunk": "http://o3d.googlecode.com/svn/trunk",
  "python_26_rev": "53573",
  "tools_rev": "5218",
  "wix_rev": "153",
}

deps = {
  "base":
    Var("chromium_trunk") + "/src/base@" + Var("chrome_rev"),
  "build":
    Var("chromium_trunk") + "/src/build@" + Var("chrome_rev"),
  "chrome/test/pyautolib":
    Var("chromium_trunk") + "/src/chrome/test/pyautolib@" + Var("chrome_rev"),
  "gpu/command_buffer":
    Var("chromium_trunk") + "/src/gpu/command_buffer@" + Var("chrome_rev"),
  "gpu/common":
    Var("chromium_trunk") + "/src/gpu/common@" + Var("chrome_rev"),
  "gpu/demos":
    Var("chromium_trunk") + "/src/gpu/demos@" + Var("chrome_rev"),
  "gpu/GLES2":
    Var("chromium_trunk") + "/src/gpu/GLES2@" + Var("chrome_rev"),
  "gpu/KHR":
    Var("chromium_trunk") + "/src/gpu/KHR@" + Var("chrome_rev"),
  "gpu/pgl":
    Var("chromium_trunk") + "/src/gpu/pgl@" + Var("chrome_rev"),
  "native_client/src/third_party/valgrind/bin":
    Var("native_client_trunk") + "/src/third_party/valgrind/bin@" +
    Var("tools_rev"),
  "net/tools/testserver":
    Var("chromium_trunk") + "/src/net/tools/testserver@" + Var("chrome_rev"),
  "ppapi":
    Var("chromium_trunk") + "/src/ppapi@" + Var("chrome_rev"),
  "testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@" + Var("gtest_rev"),
  "third_party/cloog":
    Var("native_client_trunk") + "/src/third_party/cloog@" + Var("tools_rev"),
  "third_party/gles2_book":
    Var("chromium_trunk") + "/src/third_party/gles2_book@" + Var("chrome_rev"),
  "third_party/gmp":
    Var("native_client_trunk") + "/src/third_party/gmp@" + Var("tools_rev"),
  "third_party/lcov":
    Var("chromium_trunk") + "/src/third_party/lcov@" + Var("lcov_rev"),
  "third_party/mpfr":
    Var("native_client_trunk") + "/src/third_party/mpfr@" + Var("tools_rev"),
  "third_party/mpc":
    Var("native_client_trunk") + "/src/third_party/mpc@" + Var("tools_rev"),
  "third_party/ppl":
    Var("native_client_trunk") + "/src/third_party/ppl@" + Var("tools_rev"),
  "third_party/pyftpdlib":
    Var("chromium_trunk") + "/src/third_party/pyftpdlib@" + Var("chrome_rev"),
  "third_party/pylib":
    Var("native_client_trunk") + "/src/third_party/pylib@" + Var("tools_rev"),
  "third_party/qemu":
    Var("native_client_trunk") + "/src/third_party/qemu@" + Var("tools_rev"),
  "third_party/scons-2.0.1":
    (Var("native_client_trunk") + "/src/third_party/scons-2.0.1@" +
     Var("tools_rev")),
  "third_party/tlslite":
    Var("chromium_trunk") + "/src/third_party/tlslite@" + Var("chrome_rev"),
  "third_party/zlib":
    Var("native_client_trunk") + "/src/third_party/zlib@" + Var("tools_rev"),
  "tools/gyp":
    "http://gyp.googlecode.com/svn/trunk@" + Var("gyp_rev"),
}

deps_os = {
  "win": {
    "third_party/mingw-w64/mingw/bin":
      (Var("native_client_trunk") + "/src/third_party/mingw-w64/mingw/bin@" +
       Var("tools_rev")),
    "third_party/NSIS":
      (Var("native_client_trunk") + "/src/third_party/NSIS@" +
       Var("tools_rev")),
    "third_party/python_26":
      (Var("chromium_trunk") + "/tools/third_party/python_26@" +
       Var("python_26_rev")),
    "third_party/wix_2_0_4221":
      (Var("o3d_trunk") + "/googleclient/third_party/wix_2_0_4221@" +
       Var("wix_rev")),
  },
}

hooks = [
  {
    "pattern": ".",
    "action": ["python", "native_client/build/gyp_nacl",
               "native_client/build/all.gyp"],
  },
  {
    "pattern": ".",
    "action": ["python", "native_client/build/download_toolchains.py",
               "--x86-version", Var("x86_toolchain_version"),
               "--arm-version", Var("arm_toolchain_version")],
  },
]
