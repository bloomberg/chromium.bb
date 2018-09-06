workspace(name = "com_google_quic_trace")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "com_google_protobuf",
    sha256 = "1b666f3990f66890ade9faa77d134f0369c19d3721dc8111643b56685a717319",
    strip_prefix = "protobuf-3.5.1",
    urls = ["https://github.com/google/protobuf/releases/download/v3.5.1/protobuf-all-3.5.1.zip"],
)

http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "6e16c8bc91b1310a44f3965e616383dbda48f83e8c1eaa2370a215057b00cabe",
    strip_prefix = "gflags-77592648e3f3be87d6c7123eb81cbad75f9aef5a",
    urls = [
        "https://mirror.bazel.build/github.com/gflags/gflags/archive/77592648e3f3be87d6c7123eb81cbad75f9aef5a.tar.gz",
        "https://github.com/gflags/gflags/archive/77592648e3f3be87d6c7123eb81cbad75f9aef5a.tar.gz",
    ],
)

git_repository(
    name = "com_google_absl",
    commit = "c075ad321696fa5072e097f0a51e4fe76a6fe13e",
    remote = "https://github.com/abseil/abseil-cpp.git",
)

http_archive(
    name = "sdl2",
    build_file = "sdl2.BUILD",
    sha256 = "e6a7c71154c3001e318ba7ed4b98582de72ff970aca05abc9f45f7cbdc9088cb",
    strip_prefix = "SDL2-2.0.8",
    urls = ["https://www.libsdl.org/release/SDL2-2.0.8.zip"],
)

http_archive(
    name = "sdl2_ttf",
    build_file = "sdl2_ttf.BUILD",
    sha256 = "ad7a7d2562c19ad2b71fa4ab2e76f9f52b3ee98096c0a7d7efbafc2617073c27",
    strip_prefix = "SDL2_ttf-2.0.14",
    urls = ["https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-2.0.14.zip"],
)

git_repository(
    name = "com_google_glog",
    commit = "55cc27b6eca3d7906fc1a920ca95df7717deb4e7",
    remote = "https://github.com/google/glog.git",
)

http_archive(
    name = "freetype",
    build_file = "freetype.BUILD",
    # We patch out some modules we don't use from freetype config file.
    patch_args = ["-p1"],
    patches = ["freetype_config.patch"],
    sha256 = "bf380e4d7c4f3b5b1c1a7b2bf3abb967bda5e9ab480d0df656e0e08c5019c5e6",
    strip_prefix = "freetype-2.9",
    urls = ["https://download.savannah.gnu.org/releases/freetype/freetype-2.9.tar.gz"],
)
