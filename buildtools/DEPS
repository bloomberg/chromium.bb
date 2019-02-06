use_relative_paths = True

vars = {
  "chromium_url": "https://chromium.googlesource.com",

  # When changing these, also update the svn revisions in deps_revisions.gni
  "clang_format_revision": "96636aa0e9f047f17447f2d45a094d0b59ed7917",
  "libcxx_revision":       "e713cc0acf1ae8b82f451bf58ebef67a46ceddfb",
  "libcxxabi_revision":    "307bb62985575b2e3216a8cfd7e122e0574f33a9",
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
