{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'SEARCH': [
    '.',
    '../utils'
  ],
  'TARGETS': [
    {
      'NAME' : 'nacl_mounts',
      'TYPE' : 'lib',
      'SOURCES' : [
        "kernel_handle.cc",
        "kernel_intercept.cc",
        "kernel_object.cc",
        "kernel_proxy.cc",
        "mount.cc",
        "mount_mem.cc",
        "mount_node.cc",
        "mount_node_dir.cc",
        "mount_node_mem.cc",
        "path.cc",
      ],
    }
  ],
  'HEADERS': [
    {
      'FILES': [
        "kernel_handle.h",
        "kernel_intercept.h",
        "kernel_object.h",
        "kernel_proxy.h",
        "mount.h",
        "mount_mem.h",
        "mount_node_dir.h",
        "mount_node.h",
        "mount_node_mem.h",
        "osdirent.h",
        "osstat.h",
        "ostypes.h",
        "path.h"
      ],
      'DEST': 'include/nacl_mounts',
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
  'NAME': 'nacl_mounts',
  'EXPERIMENTAL': True
}
