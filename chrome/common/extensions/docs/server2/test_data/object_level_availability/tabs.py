# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from extensions_paths import CHROME_EXTENSIONS, SERVER2
from test_file_system import MoveAllTo
from test_util import ReadFile

FAKE_TABS_IDL = '\n'.join([
  '// Copyleft stuff.',
  '',
  '// Some description here.',
  'namespace fakeTabs {',
  '  dictionary WasImplicitlyInlinedType {};',
  '  interface Functions {',
  '    static void myFunc(WasImplicitlyInlinedType arg);',
  '    static void anotherFunc(WasImplicitlyInlinedType arg);',
  '  };',
  '};'])

FAKE_TABS_WITH_INLINING_IDL = '\n'.join([
  '// Copyleft stuff.',
  '',
  '// Some description here.',
  'namespace fakeTabs {',
  '  dictionary WasImplicitlyInlinedType {};',
  '  interface Functions {',
  '    static void myFunc(WasImplicitlyInlinedType arg);',
  '  };',
  '};'])

TABS_SCHEMA_BRANCHES = MoveAllTo(CHROME_EXTENSIONS, {
  'trunk': {
    'docs': {
      'templates': {
        'json': {
          'api_availabilities.json': '{}',
          'intro_tables.json': '{}'
        }
      }
    },
    'api': {
      '_api_features.json': json.dumps({
        'tabs.scheduledFunc': {
          'channel': 'stable'
        }
      }),
      '_manifest_features.json': '{}',
      '_permission_features.json': '{}',
      'fake_tabs.idl': FAKE_TABS_IDL,
      'tabs.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'type': 'any',
            'properties': {
              'url': {
                'type': 'any'
              },
              'index': {
                'type': 'any'
              },
              'selected': {
                'type': 'any'
              },
              'id': {
                'type': 'any'
              },
              'windowId': {
                'type': 'any'
              }
            }
          },
          {
            'id': 'InlinedType',
            'type': 'any',
            'inline_doc': True
          },
          {
            'id': 'InjectDetails',
            'type': 'any',
            'properties': {
              'allFrames': {
                'type': 'any'
              },
              'code': {
                'type': 'any'
              },
              'file': {
                'type':'any'
              }
            }
          },
          {
            'id': 'DeprecatedType',
            'type': 'any',
            'deprecated': 'This is deprecated'
          }
        ],
        'properties': {
          'fakeTabsProperty1': {
            'type': 'any'
          },
          'fakeTabsProperty2': {
            'type': 'any'
          },
          'fakeTabsProperty3': {
            'type': 'any'
          }
        },
        'functions': [
          {
            'name': 'getCurrent',
            'parameters': [
              {
                'name': 'callback',
                'type': 'function',
                'parameters': [
                  {
                    'name': 'tab',
                    'type': 'any'
                  }
                ]
              }
            ]
          },
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'type': 'function',
                'parameters': [
                  {
                    'name': 'tab',
                    'type': 'any'
                  }
                ]
              },
              {
                'name': 'tabId',
                'type': 'any'
              }
            ]
          },
          {
            'name': 'scheduledFunc',
            'parameters': []
          }
        ],
        'events': [
          {
            'name': 'onActivated',
            'type': 'event',
            'parameters': [
              {
                'name': 'activeInfo',
                'type': 'any',
                'properties': {
                  'tabId': {
                    'type': 'any'
                  },
                  'windowId': {
                    'type': 'any'
                  }
                }
              }
            ]
          },
          {
            'name': 'onUpdated',
            'type': 'event',
            'parameters': [
              {
                'name': 'tabId',
                'type': 'any'
              },
              {
                'name': 'tab',
                'type': 'any'
              },
              {
                'name': 'changeInfo',
                'type': 'any',
                'properties': {
                  'pinned': {
                    'type': 'any'
                  },
                  'status': {
                    'type': 'any'
                  }
                }
              }
            ]
          }
        ]
      }])
    }
  },
  '1500': {
    'api': {
      '_api_features.json': json.dumps({
        'tabs.scheduledFunc': {
          'channel': 'stable'
        }
      }),
      '_manifest_features.json': "{}",
      '_permission_features.json': "{}",
      'fake_tabs.idl': FAKE_TABS_IDL,
      'tabs.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'properties': {
              'url': {},
              'index': {},
              'selected': {},
              'id': {},
              'windowId': {}
            }
          },
          {
            'id': 'InjectDetails',
            'properties': {
              'allFrames': {},
              'code': {},
              'file': {}
            }
          },
          {
            'id': 'DeprecatedType',
            'deprecated': 'This is deprecated'
          }
        ],
        'properties': {
          'fakeTabsProperty1': {},
          'fakeTabsProperty2': {}
        },
        'functions': [
          {
            'name': 'getCurrent',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          },
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              },
              {
                'name': 'tabId'
              }
            ]
          },
          {
            'name': 'scheduledFunc',
            'parameters': []
          }
        ],
        'events': [
          {
            'name': 'onActivated',
            'parameters': [
              {
                'name': 'activeInfo',
                'properties': {
                  'tabId': {},
                  'windowId': {}
                }
              }
            ]
          },
          {
            'name': 'onUpdated',
            'parameters': [
              {
                'name': 'tabId'
              },
              {
                'name': 'tab'
              },
              {
                'name': 'changeInfo',
                'properties': {
                  'pinned': {},
                  'status': {}
                }
              }
            ]
          }
        ]
      }])
    }
  },
  '1453': {
    'api': {
      '_api_features.json': "{}",
      '_manifest_features.json': "{}",
      '_permission_features.json': "{}",
      'fake_tabs.idl': FAKE_TABS_IDL,
      'tabs.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'properties': {
              'url': {},
              'index': {},
              'selected': {},
              'id': {},
              'windowId': {}
            }
          },
          {
            'id': 'InjectDetails',
            'properties': {
              'allFrames': {},
              'code': {},
              'file': {}
            }
          },
          {
            'id': 'DeprecatedType',
            'deprecated': 'This is deprecated'
          }
        ],
        'properties': {
          'fakeTabsProperty1': {},
          'fakeTabsProperty2': {}
        },
        'functions': [
          {
            'name': 'getCurrent',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          },
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              },
              {
                'name': 'tabId'
              }
            ]
          }
        ],
        'events': [
          {
            'name': 'onActivated',
            'parameters': [
              {
                'name': 'activeInfo',
                'properties': {
                  'tabId': {}
                }
              }
            ]
          },
          {
            'name': 'onUpdated',
            'parameters': [
              {
                'name': 'tabId'
              },
              {
                'name': 'changeInfo',
                'properties': {
                  'pinned': {},
                  'status': {}
                }
              }
            ]
          }
        ]
      }])
    }
  },
  '1410': {
    'api': {
      '_manifest_features.json': "{}",
      '_permission_features.json': "{}",
      'fake_tabs.idl': FAKE_TABS_IDL,
      'tabs.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'properties': {
              'url': {},
              'index': {},
              'selected': {},
              'id': {},
              'windowId': {}
            }
          },
          {
            'id': 'InjectDetails',
            'properties': {
              'allFrames': {},
              'code': {},
              'file': {}
            }
          },
          {
            'id': 'DeprecatedType',
            'deprecated': 'This is deprecated'
          }
        ],
        'properties': {
          'fakeTabsProperty1': {},
          'fakeTabsProperty2': {}
        },
        'functions': [
          {
            'name': 'getCurrent',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          },
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          }
        ],
        'events': [
          {
            'name': 'onUpdated',
            'parameters': [
              {
                'name': 'tabId'
              },
              {
                'name': 'changeInfo',
                'properties': {
                  'pinned': {},
                  'status': {}
                }
              }
            ]
          }
        ]
      }])
    }
  },
  '1364': {
    'api': {
      '_manifest_features.json': "{}",
      '_permission_features.json': "{}",
      'fake_tabs.idl': FAKE_TABS_WITH_INLINING_IDL,
      'tabs.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'properties': {
              'url': {},
              'index': {},
              'selected': {},
              'id': {},
              'windowId': {}
            }
          },
          {
            'id': 'InjectDetails',
            'properties': {
              'allFrames': {}
            }
          },
          {
            'id': 'DeprecatedType',
          }
        ],
        'properties': {
          'fakeTabsProperty1': {},
          'fakeTabsProperty2': {}
        },
        'functions': [
          {
            'name': 'getCurrent',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          },
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          }
        ],
        'events': [
          {
            'name': 'onUpdated',
            'parameters': [
              {
                'name': 'tabId'
              },
              {
                'name': 'changeInfo',
                'properties': {
                  'pinned': {},
                  'status': {}
                }
              }
            ]
          }
        ]
      }])
    }
  },
  '1312': {
    'api': {
      '_manifest_features.json': "{}",
      '_permission_features.json': "{}",
      'tabs.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'properties': {
              'url': {},
              'index': {},
              'selected': {},
              'id': {},
              'windowId': {}
            }
          }
        ],
        'properties': {
          'fakeTabsProperty1': {},
          'fakeTabsProperty2': {}
        },
        'functions': [
          {
            'name': 'getCurrent',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          },
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          }
        ],
        'events': [
          {
            'name': 'onUpdated',
            'parameters': [
              {
                'name': 'tabId'
              },
              {
                'name': 'changeInfo',
                'properties': {
                  'pinned': {},
                  'status': {}
                }
              }
            ]
          }
        ]
      }])
    }
  },
  '1271': {
    'api': {
      '_manifest_features.json': "{}",
      '_permission_features.json': "{}",
      'tabs.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'properties': {
              'url': {},
              'index': {},
              'selected': {},
              'id': {},
              'windowId': {}
            }
          }
        ],
        'properties': {
          'fakeTabsProperty1': {},
          'fakeTabsProperty2': {}
        },
        'functions': [
          {
            'name': 'getCurrent',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          },
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          }
        ],
        'events': [
          {
            'name': 'onUpdated',
            'parameters': [
              {
                'name': 'tabId'
              },
              {
                'name': 'changeInfo',
                'properties': {
                  'pinned': {},
                  'status': {}
                }
              }
            ]
          }
        ]
      }])
    }
  },
  '1229': {
    'api': {
      '_manifest_features.json': "{}",
      '_permission_features.json': "{}",
      'tabs.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'properties': {
              'url': {},
              'index': {},
              'selected': {},
              'id': {},
              'windowId': {}
            }
          }
        ],
        'properties': {
          'fakeTabsProperty1': {},
          'fakeTabsProperty2': {}
        },
        'functions': [
          {
            'name': 'getCurrent',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          },
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          }
        ],
        'events': [
          {
            'name': 'onUpdated',
            'parameters': [
              {
                'name': 'tabId'
              },
              {
                'name': 'changeInfo',
                'properties': {
                  'pinned': {},
                  'status': {}
                }
              }
            ]
          }
        ]
      }])
    }
  },
  '1180': {
    'api': {
      '_manifest_features.json': "{}",
      '_permission_features.json': "{}",
      'tabs.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'properties': {
              'url': {},
              'index': {},
              'selected': {},
              'id': {}
            }
          }
        ],
        'properties': {
          'fakeTabsProperty1': {},
          'fakeTabsProperty2': {}
        },
        'functions': [
          {
            'name': 'getCurrent',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          },
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          }
        ],
        'events': [
          {
            'name': 'onUpdated',
            'parameters': [
              {
                'name': 'tabId'
              },
              {
                'name': 'changeInfo',
                'properties': {
                  'pinned': {},
                  'status': {}
                }
              }
            ]
          }
        ]
      }])
    }
  },
  '1132': {
    'api': {
      '_manifest_features.json': "{}",
      '_permission_features.json': "{}",
      'tabs.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'properties': {
              'url': {},
              'index': {},
              'id': {}
            }
          }
        ],
        'properties': {
          'fakeTabsProperty1': {},
          'fakeTabsProperty2': {}
        },
        'functions': [
          {
            'name': 'getCurrent',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          },
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          }
        ],
        'events': [
          {
            'name': 'onUpdated',
            'parameters': [
              {
                'name': 'tabId'
              },
              {
                'name': 'changeInfo',
                'properties': {
                  'pinned': {},
                  'status': {}
                }
              }
            ]
          }
        ]
      }])
    }
  },
  '1084': {
    'api': {
      'tabs.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'properties': {
              'url': {},
              'index': {},
              'id': {}
            }
          }
        ],
        'properties': {
          'fakeTabsProperty1': {},
          'fakeTabsProperty2': {}
        },
        'functions': [
          {
            'name': 'getCurrent',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          },
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          }
        ],
        'events': [
          {
            'name': 'onUpdated',
            'parameters': [
              {
                'name': 'tabId'
              },
              {
                'name': 'changeInfo',
                'properties': {
                  'pinned': {},
                  'status': {}
                }
              }
            ]
          }
        ]
      }])
    }
  },
  '1025': {
    'api': {
      'tabs.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'properties': {
              'url': {},
              'index': {},
              'id': {}
            }
          }
        ],
        'properties': {
          'fakeTabsProperty1': {},
          'fakeTabsProperty2': {}
        },
        'functions': [
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          }
        ],
        'events': [
          {
            'name': 'onUpdated',
            'parameters': [
              {
                'name': 'tabId'
              },
              {
                'name': 'changeInfo',
                'properties': {
                  'pinned': {},
                  'status': {}
                }
              }
            ]
          }
        ]
      }])
    }
  },
  '963': {
    'api': {
      'extension_api.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'properties': {
              'url': {},
              'id': {}
            }
          }
        ],
        'properties': {
          'fakeTabsProperty1': {},
          'fakeTabsProperty2': {}
        },
        'functions': [
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          }
        ],
        'events': [
          {
            'name': 'onUpdated',
            'parameters': [
              {
                'name': 'tabId'
              },
              {
                'name': 'changeInfo',
                'properties': {
                  'pinned': {},
                  'status': {}
                }
              }
            ]
          }
        ]
      }])
    }
  },
  '912': {
    'api': {
      'extension_api.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'properties': {
              'url': {},
              'id': {}
            }
          }
        ],
        'properties': {
          'fakeTabsProperty1': {},
          'fakeTabsProperty2': {}
        },
        'functions': [
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          }
        ],
        'events': [
          {
            'name': 'onUpdated',
            'parameters': [
              {
                'name': 'tabId'
              }
            ]
          }
        ]
      }])
    }
  },
  '874': {
    'api': {
      'extension_api.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'properties': {
              'url': {},
              'id': {}
            }
          }
        ],
        'properties': {
          'fakeTabsProperty1': {},
          'fakeTabsProperty2': {}
        },
        'functions': [
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          }
        ],
        'events': [
          {
            'name': 'onUpdated',
            'parameters': [
              {
                'name': 'tabId'
              }
            ]
          }
        ]
      }])
    }
  },
  '835': {
    'api': {
      'extension_api.json': json.dumps([{
        'namespace': 'tabs',
        'types': [
          {
            'id': 'Tab',
            'properties': {
              'url': {},
              'id': {}
            }
          }
        ],
        'properties': {
          'fakeTabsProperty1': {}
        },
        'functions': [
          {
            'name': 'get',
            'parameters': [
              {
                'name': 'callback',
                'parameters': [
                  {
                    'name': 'tab'
                  }
                ]
              }
            ]
          }
        ],
        'events': [
          {
            'name': 'onUpdated',
            'parameters': [
              {
                'name': 'tabId'
              }
            ]
          }
        ]
      }])
    }
  },
  '782': {
    'api': {
      'extension_api.json': "{}"
    }
  }
})
