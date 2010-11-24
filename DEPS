vars = {
  "chromium_trunk": "http://src.chromium.org/svn/trunk",
  "native_client_trunk": "http://src.chromium.org/native_client/trunk",
  "o3d_trunk": "http://o3d.googlecode.com/svn/trunk",

  "x86_toolchain_version": "3746",
  "arm_toolchain_version": "3761",

  "breakpad_rev": "452",
  "chrome_rev": "66781",
  "nacl_breakpad_gyp_rev": "1806",
  "gyp_rev": "856",
  "gtest_rev": "267",
  "tools_rev": "3748",
  "python_26_rev": "53573",
  "wix_rev": "153",
  "lcov_rev": "54822",
}

deps = {
  "tools/gyp":
    "http://gyp.googlecode.com/svn/trunk@" + Var("gyp_rev"),
  "build":
    Var("chromium_trunk") + "/src/build@" + Var("chrome_rev"),
  "site_scons":
    Var("chromium_trunk") + "/src/site_scons@" + Var("chrome_rev"),
  "testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@" + Var("gtest_rev"),
  "third_party/binutils":
    (Var("native_client_trunk") + "/src/third_party/binutils@" +
     Var("tools_rev")),
  "breakpad":
    Var("native_client_trunk")
    + "/src/native_client/src/third_party/breakpad@"
    + Var("nacl_breakpad_gyp_rev"),
  "breakpad/src":
    "http://google-breakpad.googlecode.com/svn/trunk/src@" +
    Var("breakpad_rev"),
  "third_party/gcc":
    Var("native_client_trunk") + "/src/third_party/gcc@" + Var("tools_rev"),
  "third_party/gdb":
    Var("native_client_trunk") + "/src/third_party/gdb@" + Var("tools_rev"),
  "third_party/gmp":
    Var("native_client_trunk") + "/src/third_party/gmp@" + Var("tools_rev"),
  "third_party/qemu":
    Var("native_client_trunk") + "/src/third_party/qemu@" + Var("tools_rev"),
  "third_party/mpfr":
    Var("native_client_trunk") + "/src/third_party/mpfr@" + Var("tools_rev"),
  "third_party/newlib":
    Var("native_client_trunk") + "/src/third_party/newlib@" + Var("tools_rev"),
  "third_party/scons":
    Var("native_client_trunk") + "/src/third_party/scons@" + Var("tools_rev"),
  "third_party/sdl":
    Var("native_client_trunk") + "/src/third_party/sdl@" + Var("tools_rev"),
  "third_party/selenium":
    (Var("native_client_trunk") + "/src/third_party/selenium@" +
     Var("tools_rev")),
  "third_party/zlib":
    Var("native_client_trunk") + "/src/third_party/zlib@" + Var("tools_rev"),
  "third_party/npapi/bindings":  # Includes needed for using Chromium NPAPI
    (Var("chromium_trunk") + "/src/third_party/npapi/bindings@" +
     Var("chrome_rev")),
  "base":
    Var("chromium_trunk") + "/src/base@" + Var("chrome_rev"),
  "gpu/GLES2":
    Var("chromium_trunk") + "/src/gpu/GLES2@" + Var("chrome_rev"),
  "gpu/KHR":
    Var("chromium_trunk") + "/src/gpu/KHR@" + Var("chrome_rev"),
  "gpu/pgl":
    Var("chromium_trunk") + "/src/gpu/pgl@" + Var("chrome_rev"),
  "gpu/command_buffer":
    Var("chromium_trunk") + "/src/gpu/command_buffer@" + Var("chrome_rev"),
  "ppapi":
    Var("chromium_trunk") + "/src/ppapi@" + Var("chrome_rev"),
  "third_party/lcov":
    Var("chromium_trunk") + "/src/third_party/lcov@" + Var("lcov_rev"),
}

deps_os = {
  "win": {
    "third_party/python_26":
      (Var("chromium_trunk") + "/tools/third_party/python_26@" +
       Var("python_26_rev")),
    "third_party/wix_2_0_4221":
      (Var("o3d_trunk") + "/googleclient/third_party/wix_2_0_4221@" +
       Var("wix_rev")),
    "third_party/mingw-w64/mingw/bin":
      (Var("native_client_trunk") + "/src/third_party/mingw-w64/mingw/bin@" +
       Var("tools_rev")),
    "third_party/NSIS":
      (Var("native_client_trunk") + "/src/third_party/NSIS@" +
       Var("tools_rev")),
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
    "action": ["python", "native_client/tools/multipy.py",
               "native_client/src/shared/ppapi_proxy/ppapi_proxy.mpy",
               "GENDIR=srpc",
               "SRCDIR=native_client/src/shared/ppapi_proxy",
               "TOOLDIR=native_client/tools"],
  },
  {
    "pattern": "DEPS$",
    "action": ["python", "native_client/build/download_toolchains.py",
               "--x86-version", Var("x86_toolchain_version"),
               "--arm-version", Var("arm_toolchain_version")],
  },
]

