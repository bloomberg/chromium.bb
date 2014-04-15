include_rules = [
  "+crypto",
  "+gin/public",
  "+jni",
  "+third_party/apple_apsl",
  "+third_party/libevent",
  "+third_party/nss",
  "+third_party/zlib",
  "+sdch/open-vcdiff",
  "+v8",

  # Most of net should not depend on icu, to keep size down when built as a
  # library.
  "-base/i18n",
  "-third_party/icu",
]

specific_include_rules = {
  # Within net, only used by file: requests.
  "directory_lister(\.cc|_unittest\.cc)": [
    "+base/i18n",
  ],

  # Within net, only used by file: requests.
  "filename_util\.cc": [
    "+base/i18n",
  ],
  
  # Functions largely not used by the rest of net.
  "net_util_icu\.cc": [
    "+base/i18n",
    "+third_party/icu",
  ],
  
  # Uses icu for debug logging only.
  "network_time_notifier\.cc": [
    "+base/i18n",
  ],

  # Only use icu for string conversions.
  "escape_unittest\.cc": [
    "+base/i18n/icu_string_conversions.h",
  ],
  "http_auth_handler_basic\.cc": [
    "+base/i18n/icu_string_conversions.h",
  ],
  "http_auth_handler_digest\.cc": [
    "+base/i18n/icu_string_conversions.h",
  ],
  "proxy_script_fetcher_impl\.cc": [
    "+base/i18n/icu_string_conversions.h",
  ],
  "x509_cert_types_mac\.cc": [
    "+base/i18n/icu_string_conversions.h",
  ],
  "http_content_disposition\.cc": [
    "+base/i18n/icu_string_conversions.h",
    "+third_party/icu",
  ],
  "websocket_channel\.h": [
    "+base/i18n",
  ],

  "ftp_util\.cc": [
    "+base/i18n",
    "+third_party/icu",
  ],
  "ftp_directory_listing_parser\.cc": [
    "+base/i18n",
  ],
}

skip_child_includes = [
  "third_party",
  "tools/flip_server",
]
