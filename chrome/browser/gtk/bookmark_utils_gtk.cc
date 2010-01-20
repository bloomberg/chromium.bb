// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_utils_gtk.h"

#include "app/gfx/gtk_util.h"
#include "app/gtk_dnd_util.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/common/gtk_util.h"

namespace {

// Used in gtk_selection_data_set(). (I assume from this parameter that gtk has
// to some really exotic hardware...)
const int kBitsInAByte = 8;

// Maximum number of characters on a bookmark button.
const size_t kMaxCharsOnAButton = 15;

// Max size of each component of the button tooltips.
const size_t kMaxTooltipTitleLength = 100;
const size_t kMaxTooltipURLLength = 400;

// Only used for the background of the drag widget.
const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xe6, 0xed, 0xf4);

// Padding between the chrome button highlight border and the contents (favicon,
// text).
// TODO(estade): we need to adjust the top and bottom padding, but first we need
// to give the bookmark bar more space (at the expense of the toolbar).
const int kButtonPaddingTop = 0;
const int kButtonPaddingBottom = 0;
const int kButtonPaddingLeft = 2;
const int kButtonPaddingRight = 0;

void* AsVoid(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

}  // namespace

namespace bookmark_utils {

const char kBookmarkNode[] = "bookmark-node";

// Spacing between the buttons on the bar.
const int kBarButtonPadding = 4;

GdkPixbuf* GetPixbufForNode(const BookmarkNode* node, BookmarkModel* model,
                            bool native) {
  GdkPixbuf* pixbuf;

  if (node->is_url()) {
    if (model->GetFavIcon(node).width() != 0) {
      pixbuf = gfx::GdkPixbufFromSkBitmap(&model->GetFavIcon(node));
    } else {
      pixbuf = GtkThemeProvider::GetDefaultFavicon(native);
      g_object_ref(pixbuf);
    }
  } else {
    pixbuf = GtkThemeProvider::GetFolderIcon(native);
    g_object_ref(pixbuf);
  }

  return pixbuf;
}

GtkWidget* GetDragRepresentation(const BookmarkNode* node,
                                 BookmarkModel* model,
                                 GtkThemeProvider* provider) {
  // Build a windowed representation for our button.
  GtkWidget* window = gtk_window_new(GTK_WINDOW_POPUP);
  if (!provider->UseGtkTheme()) {
    // TODO(erg): Theme wise, which color should I be picking here?
    // COLOR_BUTTON_BACKGROUND doesn't match the default theme!
    gtk_widget_modify_bg(window, GTK_STATE_NORMAL, &kBackgroundColor);
  }
  gtk_widget_realize(window);

  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
  gtk_container_add(GTK_CONTAINER(window), frame);
  gtk_widget_show(frame);

  GtkWidget* floating_button = provider->BuildChromeButton();
  bookmark_utils::ConfigureButtonForNode(node, model, floating_button,
                                         provider);
  gtk_container_add(GTK_CONTAINER(frame), floating_button);
  gtk_widget_show(floating_button);

  return window;
}

void ConfigureButtonForNode(const BookmarkNode* node, BookmarkModel* model,
                            GtkWidget* button, GtkThemeProvider* provider) {
  GtkWidget* former_child = gtk_bin_get_child(GTK_BIN(button));
  if (former_child)
    gtk_container_remove(GTK_CONTAINER(button), former_child);

  std::string tooltip = BuildTooltipFor(node);
  if (!tooltip.empty())
    gtk_widget_set_tooltip_markup(button, tooltip.c_str());

  // We pack the button manually (rather than using gtk_button_set_*) so that
  // we can have finer control over its label.
  GdkPixbuf* pixbuf = bookmark_utils::GetPixbufForNode(node, model,
                                                       provider->UseGtkTheme());
  GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
  g_object_unref(pixbuf);

  GtkWidget* box = gtk_hbox_new(FALSE, kBarButtonPadding);
  gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);

  std::string label_string = WideToUTF8(node->GetTitle());
  if (!label_string.empty()) {
    GtkWidget* label = gtk_label_new(label_string.c_str());

    // Ellipsize long bookmark names.
    if (node != model->other_node()) {
      gtk_label_set_max_width_chars(GTK_LABEL(label), kMaxCharsOnAButton);
      gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    }

    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    SetButtonTextColors(label, provider);
  }

  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  // If we are not showing the label, don't set any padding, so that the icon
  // will just be centered.
  if (label_string.c_str()) {
    gtk_alignment_set_padding(GTK_ALIGNMENT(alignment),
        kButtonPaddingTop, kButtonPaddingBottom,
        kButtonPaddingLeft, kButtonPaddingRight);
  }
  gtk_container_add(GTK_CONTAINER(alignment), box);
  gtk_container_add(GTK_CONTAINER(button), alignment);

  g_object_set_data(G_OBJECT(button), bookmark_utils::kBookmarkNode,
                    AsVoid(node));

  gtk_widget_show_all(alignment);
}

std::string BuildTooltipFor(const BookmarkNode* node) {
  const std::string& url = node->GetURL().possibly_invalid_spec();
  const std::string& title = WideToUTF8(node->GetTitle());

  std::string truncated_url = WideToUTF8(l10n_util::TruncateString(
      UTF8ToWide(url), kMaxTooltipURLLength));
  gchar* escaped_url_cstr = g_markup_escape_text(truncated_url.c_str(),
                                                 truncated_url.size());
  std::string escaped_url(escaped_url_cstr);
  g_free(escaped_url_cstr);

  std::string tooltip;
  if (url == title || title.empty()) {
    return escaped_url;
  } else {
    std::string truncated_title = WideToUTF8(l10n_util::TruncateString(
      node->GetTitle(), kMaxTooltipTitleLength));
    gchar* escaped_title_cstr = g_markup_escape_text(truncated_title.c_str(),
                                                     truncated_title.size());
    std::string escaped_title(escaped_title_cstr);
    g_free(escaped_title_cstr);

    if (!escaped_url.empty())
      return std::string("<b>") + escaped_title + "</b>\n" + escaped_url;
    else
      return std::string("<b>") + escaped_title + "</b>";
  }
}

const BookmarkNode* BookmarkNodeForWidget(GtkWidget* widget) {
  return reinterpret_cast<const BookmarkNode*>(
      g_object_get_data(G_OBJECT(widget), bookmark_utils::kBookmarkNode));
}

void SetButtonTextColors(GtkWidget* label, GtkThemeProvider* provider) {
  if (provider->UseGtkTheme()) {
    gtk_util::SetLabelColor(label, NULL);
  } else {
    GdkColor color = provider->GetGdkColor(
        BrowserThemeProvider::COLOR_BOOKMARK_TEXT);
    gtk_util::SetLabelColor(label, &color);
  }
}

// DnD-related -----------------------------------------------------------------

void WriteBookmarkToSelection(const BookmarkNode* node,
                              GtkSelectionData* selection_data,
                              guint target_type,
                              Profile* profile) {
  DCHECK(node);
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  WriteBookmarksToSelection(nodes, selection_data, target_type, profile);
}

void WriteBookmarksToSelection(const std::vector<const BookmarkNode*>& nodes,
                               GtkSelectionData* selection_data,
                               guint target_type,
                               Profile* profile) {
  switch (target_type) {
    case GtkDndUtil::CHROME_BOOKMARK_ITEM: {
      BookmarkDragData data(nodes);
      Pickle pickle;
      data.WriteToPickle(profile, &pickle);

      gtk_selection_data_set(selection_data, selection_data->target,
                             kBitsInAByte,
                             static_cast<const guchar*>(pickle.data()),
                             pickle.size());
      break;
    }
    case GtkDndUtil::NETSCAPE_URL: {
      // _NETSCAPE_URL format is URL + \n + title.
      std::string utf8_text = nodes[0]->GetURL().spec() + "\n" + UTF16ToUTF8(
          nodes[0]->GetTitleAsString16());
      gtk_selection_data_set(selection_data,
                             selection_data->target,
                             kBitsInAByte,
                             reinterpret_cast<const guchar*>(utf8_text.c_str()),
                             utf8_text.length());
      break;
    }
    case GtkDndUtil::TEXT_URI_LIST: {
      gchar** uris = reinterpret_cast<gchar**>(malloc(sizeof(gchar*) *
                                               (nodes.size() + 1)));
      for (size_t i = 0; i < nodes.size(); ++i) {
        // If the node is a folder, this will be empty. TODO(estade): figure out
        // if there are any ramifications to passing an empty URI. After a
        // little testing, it seems fine.
        const GURL& url = nodes[i]->GetURL();
        // This const cast should be safe as gtk_selection_data_set_uris()
        // makes copies.
        uris[i] = const_cast<gchar*>(url.spec().c_str());
      }
      uris[nodes.size()] = NULL;

      gtk_selection_data_set_uris(selection_data, uris);
      free(uris);
      break;
    }
    case GtkDndUtil::TEXT_PLAIN: {
      gtk_selection_data_set_text(selection_data,
                                  nodes[0]->GetURL().spec().c_str(), -1);
      break;

    }
    default: {
      DLOG(ERROR) << "Unsupported drag get type!";
    }
  }
}

std::vector<const BookmarkNode*> GetNodesFromSelection(
    GdkDragContext* context,
    GtkSelectionData* selection_data,
    guint target_type,
    Profile* profile,
    gboolean* delete_selection_data,
    gboolean* dnd_success) {
  *delete_selection_data = FALSE;
  *dnd_success = FALSE;

  if ((selection_data != NULL) && (selection_data->length >= 0)) {
    if (context->action == GDK_ACTION_MOVE) {
      *delete_selection_data = TRUE;
    }

    switch (target_type) {
      case GtkDndUtil::CHROME_BOOKMARK_ITEM: {
        *dnd_success = TRUE;
        Pickle pickle(reinterpret_cast<char*>(selection_data->data),
                      selection_data->length);
        BookmarkDragData drag_data;
        drag_data.ReadFromPickle(&pickle);
        return drag_data.GetNodes(profile);
      }
      default: {
        DLOG(ERROR) << "Unsupported drag received type: " << target_type;
      }
    }
  }

  return std::vector<const BookmarkNode*>();
}

bool CreateNewBookmarkFromNamedUrl(GtkSelectionData* selection_data,
    BookmarkModel* model, const BookmarkNode* parent, int idx) {
  GURL url;
  string16 title;
  if (!GtkDndUtil::ExtractNamedURL(selection_data, &url, &title))
    return false;

  model->AddURL(parent, idx, UTF16ToWideHack(title), url);
  return true;
}

bool CreateNewBookmarksFromURIList(GtkSelectionData* selection_data,
    BookmarkModel* model, const BookmarkNode* parent, int idx) {
  std::vector<GURL> urls;
  GtkDndUtil::ExtractURIList(selection_data, &urls);
  for (size_t i = 0; i < urls.size(); ++i) {
    std::string title = GetNameForURL(urls[i]);
    model->AddURL(parent, idx++, UTF8ToWide(title), urls[i]);
  }
  return true;
}

}  // namespace bookmark_utils
