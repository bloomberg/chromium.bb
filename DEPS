# DEPS files look like -*- Python -*-

vars = {
  # These revisions are slices of the chromium repository.
  # Because they come from separate sub-slices, their hashes do not match at
  # equivalent revisions. When updating them, use the roll-dep script
  # to move them to equivalent revisions. Additionally, because not all
  # directories contain commits at each revision, you will need to select
  # revisions at latest revision up to a high watermark from each slice.
  # Document the high watermark here:
  # chrome_rev: 363546
  "build_rev": "fd1a66b01ac6fc05107e8f4a0509ca89cc6dd0cf", # from cr commit position 363527
  "build_overrides_rev": "8483c5358c0e69f1fcd4269ff2cd27f8c8f019b7", # from cr commit position 362804
  "binutils_rev": "c485984110688e490052a3ef60218a26b71e207a", # from cr commit position 354325
  "libcxx_static_rev": "17acece915b87b2d53da5e8cbc2028b1300783e2", # from cr commit position 356325
  # NOTE: be sure to update clang_lib_version in SConstruct whenever
  # updating this clang_rev (e.g., if LLVM changes from version 3.7 to 3.8).
  "clang_rev": "5b7bd8a08ae5ee5262c38e01f5b703cc193074d4", # from cr commit position 363337

  # NOTE!  These five should be kept up to date with their counterparts in
  # chromium/src/DEPS.
  # Be sure to update them when updating the chromium slice revisions above.
  # (This is not essential for Breakpad, because we do not use its code
  # in the build that goes into Chromium.  But we might as well update it too.)
  # You should now use the roll-dep script in depot_tools to do this update.
  "gtest_rev": "6f8a66431cb592dad629028a50b3dd418a408c87",
  "gyp_rev": "e2313c02ad7b6d589b38fe578f5d39970a9bbc20",
  "breakpad_rev": "a4eb2e302cefff9908ec955e761fef5d813d1b00", # from svn revision 1416
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling build tools
  # and whatever else without interference from each other.
  'buildtools_revision': 'b747a9e091cb8212a62343258406eaf53a6c032e',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling lss
  # and whatever else without interference from each other.
  'lss_revision': '4fc942258fe5509549333b9487ec018e3c8c5b10',

  # Separately pinned repositories, update with roll-dep individually.
  "third_party_rev": "d4e38e5faf600b39649025e5605d6e7f94518ea7",
  "lcov_rev": "b37daf5968200da8ff520ce65c4e5bce4047dd15", # from svn revision 149720
  "gnu_binutils_rev": "f4003433b61b25666565690caf3d7a7a1a4ec436", # from svn revision 8151
  "mingw_rev": "3cc8b140b883a9fe4986d12cfd46c16a093d3527", # from svn revision 7064
  "nsis_rev": "21b6ad22daa7bfc04b9f1c1805a34622e2607a93", # from svn revision 7071
  "ragel_rev": "da42bb33f1b67c2d70b38ec1d2edf5263271b635", # from svn revision 9010
  "validator_snapshots_rev": "ef053694ef9b0d98d9bed0b9bb649963084bfc81",

  "chromium_git": "https://chromium.googlesource.com",
}

deps = {
  "breakpad":
    Var("chromium_git") + "/external/google-breakpad.git@" +
    Var("breakpad_rev"),
  "buildtools":
    Var("chromium_git") + "/chromium/buildtools.git@" +
     Var("buildtools_revision"),
  "build":
    Var("chromium_git") + "/chromium/src/build.git@" +
    Var("build_rev"),
  "build_overrides":
    Var("chromium_git") + "/chromium/src/build_overrides.git@" +
    Var("build_overrides_rev"),
  "testing/gtest":
    (Var("chromium_git") + "/external/github.com/google/googletest.git@" +
     Var("gtest_rev")),
  "third_party":
    Var("chromium_git") + "/native_client/src/third_party.git@" +
    Var("third_party_rev"),
  "validator_snapshots":
    (Var("chromium_git") + "/native_client/src/validator_snapshots.git@" +
     Var("validator_snapshots_rev")),
  "third_party/binutils":
    Var("chromium_git") + "/chromium/src/third_party/binutils.git@" +
    Var("binutils_rev"),
  "third_party/lcov":
    Var("chromium_git") + "/chromium/src/third_party/lcov.git@" +
    Var("lcov_rev"),
  "third_party/libc++-static":
    Var("chromium_git") + "/chromium/src/third_party/libc++-static.git@" +
    Var("libcxx_static_rev"),
  "third_party/lss":
    Var("chromium_git") + "/external/linux-syscall-support/lss.git@" +
    Var("lss_revision"),
  "tools/clang":
    Var("chromium_git") + "/chromium/src/tools/clang.git@" + Var("clang_rev"),
  "tools/gyp":
    Var("chromium_git") + "/external/gyp.git@" + Var("gyp_rev"),
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
}

hooks = [
  {
    # Downloads the current stable linux sysroot to build/linux/ if needed.
    # This sysroot updates at about the same rate that the chrome build deps
    # change. This script is a no-op except for linux users who are doing
    # official chrome builds or cross compiling.
    'name': 'sysroot',
    'pattern': '.',
    'action': ['python', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--running-as-hook'],
  },
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
    'name': 'win_toolchain',
    'pattern': '.',
    'action': ['python', 'build/vs_toolchain.py', 'update'],
  },
  # Pull binutils for linux, enabled debug fission for faster linking /
  # debugging when used with clang on Ubuntu Precise.
  # https://code.google.com/p/chromium/issues/detail?id=352046
  {
    'name': 'binutils',
    'pattern': 'third_party/binutils',
    'action': [
        'python',
        'third_party/binutils/download.py',
        '--ignore-if-arch=arm',
    ],
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
