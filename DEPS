# DEPS files look like -*- Python -*-

vars = {
  # These revisions are slices of the chromium repository.
  # Because they come from separate sub-slices, their hashes do not match at
  # equivalent revisions. When updating them, use the roll-dep script
  # to move them to equivalent revisions. Additionally, because not all
  # directories contain commits at each revision, you will need to select
  # revisions at latest revision up to a high watermark from each slice.
  # Document the high watermark here:
  # chrome_rev: 314402
  "valgrind_rev": "59886873b4b8258a8e0f098c23e1958e0d0c0a26", # from svn revision 231553
  "tools_valgrind_rev": "b81cde70b77b0461b18ad24616bdadb98a466647", # from svn revision 275834
  "clang_rev": "0825693c3460f23b1d78ad272999f850447ad333", # from cr commit position 313267

  # NOTE!  These five should be kept up to date with their counterparts in
  # chromium/src/DEPS.
  # Be sure to update them when updating the chromium slice revisions above.
  # (This is not essential for Breakpad, because we do not use its code
  # in the build that goes into Chromium.  But we might as well update it too.)
  # You should now use the roll-dep script in depot_tools to do this update.
  "gtest_rev": "be1868139ffe0ccd0e8e3b37292b84c821d9c8ad", # from svn revision 704
  "gyp_rev": "4d7c139b1820c5fcb993868c61f170a02cda8a40", # from svn revision 2030
  "lss_rev": "952107fa7cea0daaabead28c0e92d579bee517eb",
  "breakpad_rev": "a4eb2e302cefff9908ec955e761fef5d813d1b00", # from svn revision 1416
  "android_tools_rev": "f6e2370dff438125897bb3b3800de1ad7aa62c27",

  # Separately pinned repositories, update with roll-dep individually.
  "third_party_rev": "d4e38e5faf600b39649025e5605d6e7f94518ea7",
  "lcov_rev": "b37daf5968200da8ff520ce65c4e5bce4047dd15", # from svn revision 149720
  "gnu_binutils_rev": "f4003433b61b25666565690caf3d7a7a1a4ec436", # from svn revision 8151
  "mingw_rev": "3cc8b140b883a9fe4986d12cfd46c16a093d3527", # from svn revision 7064
  "nsis_rev": "21b6ad22daa7bfc04b9f1c1805a34622e2607a93", # from svn revision 7071
  "ragel_rev": "da42bb33f1b67c2d70b38ec1d2edf5263271b635", # from svn revision 9010
  "validator_snapshots_rev": "2d6ceadd4db38961fb3e1d45a1441319f47b2f6b",

  "chromium_git": "https://chromium.googlesource.com",

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling build tools
  # and whatever else without interference from each other.
  "buildtools_revision": "93b3d0af1b30db55ee42bd2e983f7753153217db",
}

deps = {
  "breakpad":
    Var("chromium_git") + "/external/google-breakpad.git@" +
    Var("breakpad_rev"),
  "buildtools":
    Var("chromium_git") + "/chromium/buildtools.git@" +
     Var("buildtools_revision"),
  "testing/gtest":
    Var("chromium_git") + "/external/googletest.git@" + Var("gtest_rev"),
  "third_party":
    Var("chromium_git") + "/native_client/src/third_party.git@" +
    Var("third_party_rev"),
  "validator_snapshots":
    (Var("chromium_git") + "/native_client/src/validator_snapshots.git@" +
     Var("validator_snapshots_rev")),
  "third_party/lcov":
    Var("chromium_git") + "/chromium/src/third_party/lcov.git@" +
    Var("lcov_rev"),
  "third_party/lss":
    Var("chromium_git") + "/external/linux-syscall-support/lss.git@" +
    Var("lss_rev"),
  "third_party/valgrind":
    Var("chromium_git") + "/chromium/deps/valgrind.git@" + Var("valgrind_rev"),
  "tools/clang":
    Var("chromium_git") + "/chromium/src/tools/clang.git@" + Var("clang_rev"),
  "tools/gyp":
    Var("chromium_git") + "/external/gyp.git@" + Var("gyp_rev"),
  "tools/valgrind":
    Var("chromium_git") + "/chromium/src/tools/valgrind.git@" +
    Var("tools_valgrind_rev"),
}

deps_os = {
  "win": {
    # GNU binutils assembler for x86-32.
    "third_party/gnu_binutils":
      Var("chromium_git") +
      "/native_client/deps/third_party/gnu_binutils.git@" +
      Var("gnu_binutils_rev"),
    # GNU binutils assembler for x86-64.
    "third_party/mingw-w64/mingw/bin":
      Var("chromium_git") +
      "/native_client/deps/third_party/mingw-w64/mingw/bin.git@" +
      Var("mingw_rev"),
    "third_party/NSIS":
      Var("chromium_git") + "/native_client/deps/third_party/NSIS.git@" +
      Var("nsis_rev"),
  },
  "unix": {
    # Ragel for validator_ragel
    "third_party/ragel":
      Var("chromium_git") + "/native_client/deps/third_party/ragel.git@" +
      Var("ragel_rev"),
  },
  "android": {
    "third_party/android_tools":
      Var("chromium_git") + "/android_tools.git@" + Var("android_tools_rev")
  },
}

hooks = [
  # Pull NaCl Toolchain binaries. This needs to be before running GYP below.
  {
    "pattern": ".",
    "action": ["python",
               "native_client/build/package_version/package_version.py",
               "sync", "--extract",
    ],
  },
  # Cleanup any stale package_version files.
  {
    "pattern": ".",
    "action": ["python",
               "native_client/build/package_version/package_version.py",
               "cleanup",
    ],
  },
  {
    # Update the Windows toolchain if necessary.
    "name": "win_toolchain",
    "pattern": ".",
    "action": ["python", "native_client/build/vs_toolchain.py", "update"],
  },
  # Update clang
  {
    "name": "clang",
    "pattern": ".",
    "action": ["python", "tools/clang/scripts/update.py", "--if-needed"],
  },
  # Pull GN binaries. This needs to be before running GYP below.
  {
    "name": "gn_win",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=win32",
                "--no_auth",
                "--bucket", "chromium-gn",
                "-s", "buildtools/win/gn.exe.sha1",
    ],
  },
  {
    "name": "gn_mac",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=darwin",
                "--no_auth",
                "--bucket", "chromium-gn",
                "-s", "buildtools/mac/gn.sha1",
    ],
  },
  {
    "name": "gn_linux64",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=linux*",
                "--no_auth",
                "--bucket", "chromium-gn",
                "-s", "buildtools/linux64/gn.sha1",
    ],
  },
  # Run GYP, do this last to make sure all the tools are present first.
  {
    "pattern": ".",
    "action": ["python", "native_client/build/gyp_nacl"],
  },
  # Pull clang-format binaries. Note that we currently do not support
  # clang-format on linux32
  {
    "name": "clang-format_win",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=win32",
                "--no_auth",
                "--bucket", "chromium-clang-format",
                "-s", "buildtools/win/clang-format.exe.sha1",
    ],
  },
  {
    "name": "clang-format_mac",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=darwin",
                "--no_auth",
                "--bucket", "chromium-clang-format",
                "-s", "buildtools/mac/clang-format.sha1",
    ],
  },
  {
    "name": "clang-format_linux64",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=linux*",
                "--no_auth",
                "--bucket", "chromium-clang-format",
                "-s", "buildtools/linux64/clang-format.sha1",
    ],
  },
]

include_rules = [
  "+native_client/src/include",
  "+gtest",
]
