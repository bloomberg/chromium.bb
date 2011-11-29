vars = {
  "chrome_milestone": "17",
  "chrome_rev": "109032",
  "chromium_trunk": "http://src.chromium.org/svn/trunk",
  # Get PPAPI directly from Chrome, not via the NaCl repo.
  "gmock_trunk": "http://googlemock.googlecode.com/svn/trunk/",
  "gmock_version": "382",
  "gtest_trunk": "http://googletest.googlecode.com/svn/trunk/",
  "gtest_version": "570",
  "native_client_trunk": "http://src.chromium.org/native_client/trunk",
  "native_client_version": "7141",
  # Note: The following version should exactly match the toolchain version in
  # the native_client DEPS file at version native_client_version
  # TODO(mball) find some clever way to extract this from NaCl DEPS
  "pnacl_toolchain_version": "7107",
  "x86_toolchain_version": "7098",
  # ARM is not supported, this number can stay pinned at 6645.
  "arm_trusted_toolchain_version": "6645",
  "pymox": "http://pymox.googlecode.com/svn/trunk",
  "pymox_version": "61",
}
# When we have ARM and PNaCl support, we'll need to add toolchain support
# for those too.

deps = {
  "nacl_deps":
    File(Var("native_client_trunk") + "/src/native_client/DEPS@" +
         Var("native_client_version")),
  # Please keep these in alphabetical order, by path
  "src/site_scons":
    Var("native_client_trunk") + "/src/native_client/site_scons@" +
    Var("native_client_version"),
  "src/third_party/gmock":
    Var("gmock_trunk") + "@" + Var("gmock_version"),
  "src/third_party/gtest":
    Var("gtest_trunk") + "@" + Var("gtest_version"),
  "src/third_party/native_client/base":
    Var("chromium_trunk") + "/src/base@" + Var("chrome_rev"),
  "src/third_party/native_client/build":
    Var("chromium_trunk") + "/src/build@" + Var("chrome_rev"),
  "src/third_party/native_client/gpu":
    Var("chromium_trunk") + "/src/gpu@" + Var("chrome_rev"),
  "src/third_party/native_client/native_client":
    Var("native_client_trunk") + "/src/native_client/@" +
    Var("native_client_version"),
  "src/third_party/native_client/ppapi":
    Var("chromium_trunk") + "/src/ppapi@" + Var("chrome_rev"),
  "src/third_party/pymox":
    Var("pymox") + "@" + Var("pymox_version"),
  "src/third_party/scons-2.0.1":
    Var("native_client_trunk") + "/src/third_party/scons-2.0.1@" +
    Var("native_client_version"),
  "src/third_party/native_client/third_party/pylib":
    Var("native_client_trunk") + "/src/third_party/pylib@" +
    Var("native_client_version"),
  "src/third_party/native_client/third_party/scons-2.0.1":
    Var("native_client_trunk") + "/src/third_party/scons-2.0.1@" +
    Var("native_client_version"),
  "src/third_party/native_client/third_party/simplejson":
    (Var("chromium_trunk") + "/tools/build/third_party/simplejson@" +
     Var("chrome_rev")),
  "src/third_party/native_client/tools/valgrind":
    Var("chromium_trunk") + "/src/tools/valgrind@" + Var("chrome_rev"),
}

deps_os = {
  # Please keep these in alphabetical order, by path
  "win": {
    "src/experimental/visual_studio_plugin/third_party/native_client/"
      "src/shared/gio":
      Var("native_client_trunk") + "/src/native_client/src/shared/gio@"
      + Var("native_client_version"),
    "src/experimental/visual_studio_plugin/third_party/native_client/"
      "src/shared/platform":
      Var("native_client_trunk") + "/src/native_client/src/shared/platform@"
      + Var("native_client_version"),
    "src/experimental/visual_studio_plugin/third_party/native_client/"
      "src/trusted/debug_stub":
      Var("native_client_trunk") + "/src/native_client/src/trusted/debug_stub@"
      + Var("native_client_version"),
    "src/experimental/visual_studio_plugin/third_party/native_client/"
      "src/trusted/service_runtime/include":
      Var("native_client_trunk")
      + "/src/native_client/src/trusted/service_runtime/include@"
      + Var("native_client_version"),
    "src/experimental/visual_studio_plugin/third_party/native_client/"
      "src/include":
      Var("native_client_trunk") + "/src/native_client/src/include@"
      + Var("native_client_version"),
    "src/experimental/visual_studio_plugin/third_party/native_client/"
      "src/trusted/port":
      Var("native_client_trunk") + "/src/native_client/src/trusted/port@"
      + Var("native_client_version"),
    "src/experimental/visual_studio_plugin/third_party/native_client/"
      "src/trusted/gdb_rsp":
      Var("native_client_trunk") + "/src/native_client/src/trusted/gdb_rsp@"
      + Var("native_client_version"),
    "src/third_party/native_client/third_party/mingw-w64/mingw/bin":
      From("nacl_deps", "third_party/mingw-w64/mingw/bin"),
  },
}

hooks = [
  {
    "pattern": ".",
    "action": [
        "python",
        "src/third_party/native_client/native_client/build/"
          "download_toolchains.py",
        "--x86-version", Var("x86_toolchain_version"),
        "--pnacl-version", Var("pnacl_toolchain_version"),
        "--arm-trusted-version", Var("arm_trusted_toolchain_version"),
        "--toolchain-dir", "src/toolchain",
        "--save-downloads-dir", "src/build_tools/toolchain_archives",
        ]
  },
  {
    "pattern": ".",
    "action": [
        "python",
        "src/build_tools/install_third_party.py", "--all-toolchains",
        ]
  }
]
