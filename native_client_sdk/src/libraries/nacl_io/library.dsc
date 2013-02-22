{
  # Disabled pnacl for now because it warns on using the language extension
  # typeof(...)
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win'],
  'SEARCH': [
    '.',
    'pepper',
    '../utils'
  ],
  'TARGETS': [
    {
      'NAME' : 'nacl_io',
      'TYPE' : 'lib',
      'SOURCES' : [
        "kernel_handle.cc",
        "kernel_intercept.cc",
        "kernel_object.cc",
        "kernel_proxy.cc",
        "kernel_wrap_glibc.cc",
        "kernel_wrap_newlib.cc",
        "kernel_wrap_win.cc",
        "mount.cc",
        "mount_dev.cc",
        "mount_html5fs.cc",
        "mount_http.cc",
        "mount_mem.cc",
        "mount_node.cc",
        "mount_node_dir.cc",
        "mount_node_html5fs.cc",
        "mount_node_mem.cc",
        "mount_passthrough.cc",
        "nacl_io.cc",
        "path.cc",
        "pepper_interface.cc",
        "real_pepper_interface.cc",
      ],
    }
  ],
  'HEADERS': [
    {
      'FILES': [
        "inode_pool.h",
        "kernel_handle.h",
        "kernel_intercept.h",
        "kernel_object.h",
        "kernel_proxy.h",
        "kernel_wrap.h",
        "kernel_wrap_real.h",
        "mount.h",
        "mount_dev.h",
        "mount_html5fs.h",
        "mount_http.h",
        "mount_mem.h",
        "mount_node_dir.h",
        "mount_node.h",
        "mount_node_html5fs.h",
        "mount_node_mem.h",
        "mount_passthrough.h",
        "nacl_io.h",
        "osdirent.h",
        "osinttypes.h",
        "osmman.h",
        "osstat.h",
        "ostypes.h",
        "path.h",
        "pepper_interface.h",
        "real_pepper_interface.h",
      ],
      'DEST': 'include/nacl_io',
    },
    {
      'FILES': [
        "all_interfaces.h",
        "define_empty_macros.h",
        "undef_macros.h",
      ],
      'DEST': 'include/nacl_io/pepper',
    },
    {
      'FILES': [
        "auto_lock.h",
        "macros.h",
        "ref_object.h"
      ],
      'DEST': 'include/utils',
    }
  ],
  'DEST': 'src',
  'NAME': 'nacl_io',
}
