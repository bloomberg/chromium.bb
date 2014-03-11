# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from extensions_paths import CHROME_EXTENSIONS
from test_file_system import MoveAllTo


TABS_SCHEMA_BRANCHES = MoveAllTo(CHROME_EXTENSIONS, {
  'trunk': {
    'docs': {
      'templates': {
        'json': {
          'api_availabilities.json': '{}'
        }
      }
    },
    'api': {
      '_api_features.json': '{}',
      '_manifest_features.json': '{}',
      '_permission_features.json': '{}',
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
          }
        ],
        'properties': {
          'fakeTabsProperty1': {},
          'fakeTabsProperty2': {},
          'fakeTabsProperty3': {}
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
  '1500': {
    'api': {
      '_api_features.json': "{}",
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
          },
          {
            'id': 'InjectDetails',
            'properties': {
              'allFrames': {},
              'code': {},
              'file': {}
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
