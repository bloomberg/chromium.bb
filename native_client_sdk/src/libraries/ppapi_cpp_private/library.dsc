{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'SEARCH': [
      '../../../../ppapi/cpp/private',
  ],
  'TARGETS': [
    {
      'NAME' : 'ppapi_cpp_private',
      'TYPE' : 'lib',
      'SOURCES' : [
          'ext_crx_file_system_private.cc',
          'host_resolver_private.cc',
          'net_address_private.cc',
          'tcp_socket_private.cc',
          'tcp_server_socket_private.cc',
          'udp_socket_private.cc',
          'x509_certificate_private.cc',
      ],
    }
  ],
  'DEST': 'src',
  'NAME': 'ppapi_cpp_private',
}

