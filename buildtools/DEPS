use_relative_paths = True

vars = {
  "chromium_url": "https://chromium.googlesource.com",

  # When changing these, also update the svn revisions in deps_revisions.gni
  "clang_format_revision": "96636aa0e9f047f17447f2d45a094d0b59ed7917",
  "libcxx_revision":       "9ae8fb4a3c5fef4e9d41e97bbd9397a412932ab0",
  "libcxxabi_revision":    "a50f5035629b7621e92acef968403f71b7d48553",
  "libunwind_revision":    "69d9b84cca8354117b9fe9705a4430d789ee599b",
}

deps = {
  "clang_format/script":
    Var("chromium_url") + "/chromium/llvm-project/cfe/tools/clang-format.git@" +
    Var("clang_format_revision"),
  "third_party/libc++/trunk":
    Var("chromium_url") + "/chromium/llvm-project/libcxx.git" + "@" +
    Var("libcxx_revision"),
  "third_party/libc++abi/trunk":
    Var("chromium_url") + "/chromium/llvm-project/libcxxabi.git" + "@" +
    Var("libcxxabi_revision"),
  "third_party/libunwind/trunk":
    Var("chromium_url") + "/external/llvm.org/libunwind.git" + "@" +
    Var("libunwind_revision"),
}
