# Use Visual Studio Code on Chromium code base

[TOC]

[Visual Studio Code](http://code.visualstudio.com/)
([Wikipedia](https://en.wikipedia.org/wiki/Visual_Studio_Code)) is a
multi-platform code editor that is itself based on Electron which is based on 
Chromium. Visual Studio Code has a growing community and base of installable 
extensions and themes. It works without too much setup.

## Install extensions

`ctrl+p` paste `ext install cpptools you-complete-me clang-format` then enter. 
For more extensions: https://marketplace.visualstudio.com/search?target=vscode

Highly recommend you also install your favorite keymap. 

An Example to install eclipse keymaps `ext install vscode-eclipse-keybindings`. 
You can search keymaps here. 
https://marketplace.visualstudio.com/search?target=vscode&category=Keymaps


## Settings

Open Settings `File/Code - Preferences - Settings` and add the following 
settings.

```
{
  "editor.tabSize": 2,
  "editor.rulers": [80],
  // Exclude
  "files.exclude": {
    "**/.git": true,
    "**/.svn": true,
    "**/.hg": true,
    "**/.DS_Store": true,
    "**/out": true
  },
  // YCM
  "ycmd.path": "<your_ycmd_path>",
  "ycmd.global_extra_config": 
      "<your_chromium_path>/src/tools/vim/chromium.ycm_extra_conf.py",
  "ycmd.confirm_extra_conf": false,
  "ycmd.use_imprecise_get_type": true,
  // clang-format
  "clang-format.style": "Chromium",
  "editor.formatOnSave": true
}
```

### Install auto-completion engine(ycmd)

```
$ git clone https://github.com/Valloric/ycmd.git ~/.ycmd
$ cd ~/.ycmd
$ ./build.py --clang-completer
```

## Work flow

1. `ctrl+p` open file.
2. `ctrl+o` goto symbol. `ctrl+l` goto line.
3. <code>ctrl+`</code> toggle terminal.

## Tips

### On laptop

Because we use ycmd to enable auto completion. We can disable CPP autocomplete 
and index to save battery. 

```
"C_Cpp.autocomplete": "Disabled",
"C_Cpp.addWorkspaceRootToIncludePath": false
```

### Enable Sublime-like minimap

```
"editor.minimap.enabled": true,
"editor.minimap.renderCharacters": false
```

### More

https://github.com/Microsoft/vscode-tips-and-tricks/blob/master/README.md